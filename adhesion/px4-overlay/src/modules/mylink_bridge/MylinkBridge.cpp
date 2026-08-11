/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "MylinkBridge.hpp"
#include <px4_platform_common/events.h>
#include <drivers/drv_hrt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MLB_CMD_TAKEOFF  1
#define MLB_CMD_THRUST   2
#define MLB_CMD_LAND     3
#define MLB_CMD_STOP     4

ModuleBase::Descriptor MylinkBridge::desc{task_spawn, custom_command, print_usage};

MylinkBridge::MylinkBridge() :
	ModuleParams(nullptr),
	ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::nav_and_controllers)
{
}

MylinkBridge::~MylinkBridge()
{
	closePort();
	perf_free(_loop_perf);
	perf_free(_loop_interval_perf);
}

bool MylinkBridge::init()
{
	ScheduleNow();
	return true;
}

void MylinkBridge::openPort()
{
	if (_serial_fd >= 0) { return; }

	_serial_fd = ::open(_param_mlb_port.get().c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (_serial_fd < 0) {
		PX4_ERR("mylink: open %s failed", _param_mlb_port.get().c_str());
		return;
	}

	struct termios t{};
	tcgetattr(_serial_fd, &t);
	cfmakeraw(&t);
	cfsetspeed(&t, (speed_t)_param_mlb_baudrate.get());
	t.c_cflag |= CLOCAL | CREAD;
	tcsetattr(_serial_fd, TCSANOW, &t);
	PX4_INFO("mylink: opened %s at %ld baud", _param_mlb_port.get().c_str(),
		 (long)_param_mlb_baudrate.get());
}

void MylinkBridge::closePort()
{
	if (_serial_fd >= 0) {
		::close(_serial_fd);
		_serial_fd = -1;
	}
}

void MylinkBridge::readAndParse()
{
	if (_serial_fd < 0) { return; }

	char c;
	while (::read(_serial_fd, &c, 1) == 1) {
		if (c == '\n' || c == '\r') {
			if (_read_len > 0) {
				_read_buf[_read_len] = '\0';
				PX4_INFO("mylink recv: %s", _read_buf);

				// Parse: "T nn" or "TAKEOFF" or "LAND" etc.
				char *space = strchr(_read_buf, ' ');
				int cmd = 0;
				float val = 0.0f;

				if (space) {
					*space = '\0';
					val = strtof(space + 1, nullptr);
				}

				if (strcmp(_read_buf, "TAKEOFF") == 0) {
					cmd = MLB_CMD_TAKEOFF;
				} else if (strcmp(_read_buf, "LAND") == 0) {
					cmd = MLB_CMD_LAND;
				} else if (strcmp(_read_buf, "STOP") == 0) {
					cmd = MLB_CMD_STOP;
				} else if (strcmp(_read_buf, "T") == 0) {
					cmd = MLB_CMD_THRUST;
				}

				executeCommand(cmd, val);
				_read_len = 0;
			}
		} else if (_read_len < (int)sizeof(_read_buf) - 1) {
			_read_buf[_read_len++] = c;
		}
	}
}

void MylinkBridge::publishOffboardDirectActuator()
{
	offboard_control_mode_s offboard_mode{};
	offboard_mode.direct_actuator = true;
	offboard_mode.timestamp = hrt_absolute_time();
	_offboard_control_mode_pub.publish(offboard_mode);
}

void MylinkBridge::publishActuatorMotors(float normalized_thrust)
{
	actuator_motors_s motors{};
	for (int i = 0; i < (int)actuator_motors_s::NUM_CONTROLS; i++) {
		motors.control[i] = NAN;
	}
	// M1-M4: 0 = stopped, 1 = full throttle
	motors.control[0] = normalized_thrust;
	motors.control[1] = normalized_thrust;
	motors.control[2] = normalized_thrust;
	motors.control[3] = normalized_thrust;
	motors.timestamp = hrt_absolute_time();
	motors.timestamp_sample = hrt_absolute_time();
	_actuator_motors_pub.publish(motors);
}

void MylinkBridge::sendModeCommand(uint8_t main_mode, uint8_t sub_mode)
{
	vehicle_command_s cmd{};
	cmd.command = vehicle_command_s::VEHICLE_CMD_DO_SET_MODE;
	cmd.param1 = (float)main_mode;
	cmd.param2 = (float)sub_mode;
	cmd.target_system = 1;
	cmd.source_system = 1;
	cmd.source_component = 1;
	cmd.timestamp = hrt_absolute_time();
	_vehicle_command_pub.publish(cmd);
}

void MylinkBridge::executeCommand(int cmd, float value)
{
	vehicle_status_s vehicle_status{};
	_vehicle_status_sub.copy(&vehicle_status);
	bool armed = (vehicle_status.arming_state == vehicle_status_s::ARMING_STATE_ARMED);

	switch (cmd) {
	case MLB_CMD_TAKEOFF:
		PX4_INFO("mylink: TAKEOFF");
		if (!armed) {
			vehicle_command_s arm{};
			arm.command = vehicle_command_s::VEHICLE_CMD_COMPONENT_ARM_DISARM;
			arm.param1 = 1.0f;
			arm.target_system = 1;
			arm.source_system = 1;
			arm.source_component = 1;
			arm.timestamp = hrt_absolute_time();
			_vehicle_command_pub.publish(arm);
		}
		sendModeCommand(6, 0); // offboard
		_offboard_active = true;
		break;

	case MLB_CMD_THRUST:
		PX4_INFO("mylink: THRUST %.2f", (double)value);
		if (!_offboard_active) {
			sendModeCommand(6, 0); // offboard
			_offboard_active = true;
		}
		publishOffboardDirectActuator();
		publishActuatorMotors(value);
		break;

	case MLB_CMD_LAND:
		PX4_INFO("mylink: LAND");
		_offboard_active = false;
		{
			vehicle_command_s land{};
			land.command = vehicle_command_s::VEHICLE_CMD_NAV_LAND;
			land.target_system = 1;
			land.source_system = 1;
			land.source_component = 1;
			land.timestamp = hrt_absolute_time();
			_vehicle_command_pub.publish(land);
		}
		break;

	case MLB_CMD_STOP:
		PX4_INFO("mylink: STOP");
		_offboard_active = false;
		publishActuatorMotors(0.0f);
		{
			vehicle_command_s disarm{};
			disarm.command = vehicle_command_s::VEHICLE_CMD_COMPONENT_ARM_DISARM;
			disarm.param1 = 0.0f;
			disarm.target_system = 1;
			disarm.source_system = 1;
			disarm.source_component = 1;
			disarm.timestamp = hrt_absolute_time();
			_vehicle_command_pub.publish(disarm);
		}
		break;
	}
}

void MylinkBridge::Run()
{
	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup(desc);
		return;
	}
	perf_begin(_loop_perf);
	perf_count(_loop_interval_perf);

	if (_parameter_update_sub.updated()) {
		parameter_update_s p{};
		_parameter_update_sub.copy(&p);
		updateParams();
		// Re-open port if params changed
		closePort();
	}

	if (_param_mlb_enable.get() == 0) {
		if (_offboard_active) { _offboard_active = false; }
		perf_end(_loop_perf);
		return;
	}

	openPort();
	readAndParse();

	perf_end(_loop_perf);
}

int MylinkBridge::task_spawn(int argc, char *argv[])
{
	MylinkBridge *instance = new MylinkBridge();
	if (instance) {
		desc.object.store(instance);
		desc.task_id = task_id_is_work_queue;
		if (instance->init()) { return PX4_OK; }
	} else {
		PX4_ERR("alloc failed");
	}
	delete instance;
	desc.object.store(nullptr);
	desc.task_id = -1;
	return PX4_ERROR;
}

int MylinkBridge::print_status()
{
	PX4_INFO("mylink: serial_fd=%d offboard=%d", _serial_fd, (int)_offboard_active);
	perf_print_counter(_loop_perf);
	perf_print_counter(_loop_interval_perf);
	return 0;
}

int MylinkBridge::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int MylinkBridge::print_usage(const char *reason)
{
	if (reason) { PX4_WARN("%s\n", reason); }
	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
MyLink serial bridge: reads commands from a serial port and
executes takeoff / thrust / land actions via offboard direct_actuator mode.
)DESCR_STR");
	PRINT_MODULE_USAGE_NAME("mylink_bridge", "module");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return 0;
}

extern "C" __EXPORT int mylink_bridge_main(int argc, char *argv[])
{
	return ModuleBase::main(MylinkBridge::desc, argc, argv);
}
