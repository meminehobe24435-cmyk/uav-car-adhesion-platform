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

#include "HeightCommander.hpp"
#include <px4_platform_common/events.h>
#include <math.h>

#define HC_AUX_THRESHOLD_HIGH 0.5f
#define HC_AUX_THRESHOLD_LOW  -0.5f
#define HC_ALTITUDE_TOLERANCE 0.3f

ModuleBase::Descriptor HeightCommander::desc{task_spawn, custom_command, print_usage};

HeightCommander::HeightCommander() :
	ModuleParams(nullptr),
	ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::nav_and_controllers)
{
}

HeightCommander::~HeightCommander()
{
	perf_free(_loop_perf);
	perf_free(_loop_interval_perf);
}

bool HeightCommander::init()
{
	if (!_manual_control_setpoint_sub.registerCallback()) {
		PX4_ERR("callback registration failed");
		return false;
	}
	return true;
}

float HeightCommander::getAuxChannelValue(const manual_control_setpoint_s &man_ctrl)
{
	int ch = _param_hc_channel.get();
	switch (ch) {
	case 5:  return man_ctrl.aux1;
	case 6:  return man_ctrl.aux2;
	case 7:  return man_ctrl.aux2;
	case 8:  return man_ctrl.aux3;
	case 9:  return man_ctrl.aux4;
	case 10: return man_ctrl.aux5;
	case 11: return man_ctrl.aux6;
	default: return man_ctrl.aux2;
	}
}

void HeightCommander::publishOffboardSetpoint(float target_z)
{
	offboard_control_mode_s offboard_mode{};
	offboard_mode.position = true;
	offboard_mode.velocity = false;
	offboard_mode.acceleration = false;
	offboard_mode.attitude = false;
	offboard_mode.body_rate = false;
	offboard_mode.thrust_and_torque = false;
	offboard_mode.direct_actuator = false;
	offboard_mode.timestamp = hrt_absolute_time();
	_offboard_control_mode_pub.publish(offboard_mode);

	trajectory_setpoint_s setpoint{};
	setpoint.position[0] = NAN;
	setpoint.position[1] = NAN;
	setpoint.position[2] = target_z;
	setpoint.velocity[0] = NAN;
	setpoint.velocity[1] = NAN;
	setpoint.velocity[2] = NAN;
	setpoint.acceleration[0] = NAN;
	setpoint.acceleration[1] = NAN;
	setpoint.acceleration[2] = NAN;
	setpoint.yaw = NAN;
	setpoint.yawspeed = NAN;
	setpoint.timestamp = hrt_absolute_time();
	_trajectory_setpoint_pub.publish(setpoint);
}

void HeightCommander::sendOffboardModeCommand()
{
	vehicle_command_s cmd{};
	cmd.command = vehicle_command_s::VEHICLE_CMD_DO_SET_MODE;
	cmd.param1 = 6.0f;  // PX4_CUSTOM_MAIN_MODE_OFFBOARD
	cmd.param2 = 0.0f;
	cmd.param3 = 0.0f;
	cmd.target_system = 1;
	cmd.source_system = 1;
	cmd.source_component = 1;
	cmd.timestamp = hrt_absolute_time();
	_vehicle_command_pub.publish(cmd);
}

void HeightCommander::resetToIdle()
{
	_state = IDLE;
	_home_altitude = 0.0f;
	_target_altitude = 0.0f;
	PX4_INFO("HeightCommander: reset to IDLE");
}

void HeightCommander::updateStateMachine()
{
	if (_param_hc_enable.get() == 0) { return; }

	manual_control_setpoint_s man_ctrl{};
	if (!_manual_control_setpoint_sub.copy(&man_ctrl)) { return; }
	if (!man_ctrl.valid) { return; }

	vehicle_status_s vehicle_status{};
	_vehicle_status_sub.copy(&vehicle_status);
	const bool armed = (vehicle_status.arming_state == vehicle_status_s::ARMING_STATE_ARMED);
	const bool in_offboard = (vehicle_status.nav_state == vehicle_status_s::NAVIGATION_STATE_OFFBOARD);

	if (!armed && _armed) { resetToIdle(); }
	_armed = armed;
	if (!armed) { return; }

	// Commander stick override detection
	if (_state != IDLE && !in_offboard && _was_offboard) {
		PX4_INFO("HeightCommander: stick override, resetting");
		resetToIdle();
		_was_offboard = in_offboard;
		return;
	}
	_was_offboard = in_offboard;

	vehicle_local_position_s local_pos{};
	if (!_vehicle_local_position_sub.copy(&local_pos)) {
		if (_state != IDLE) { PX4_WARN("HeightCommander: no local position"); }
		return;
	}
	if (!PX4_ISFINITE(local_pos.z) || !local_pos.z_valid) {
		if (_state != IDLE) { PX4_WARN("HeightCommander: invalid altitude"); }
		return;
	}

	float current_z = local_pos.z;
	float delta = _param_hc_delta.get();
	float aux = getAuxChannelValue(man_ctrl);
	bool aux_high = (aux > HC_AUX_THRESHOLD_HIGH);
	bool aux_low = (aux < HC_AUX_THRESHOLD_LOW);
	bool rising = aux_high && !_was_high;
	bool falling = aux_low && !_was_low;
	_was_high = aux_high;
	_was_low = aux_low;

	switch (_state) {
	case IDLE:
		if (rising) {
			_home_altitude = current_z;
			_target_altitude = current_z - delta; // NED: up = more negative
			_state = ASCENDING;
			PX4_INFO("HeightCommander: ASCENDING target=%.2f delta=%.2f",
				(double)_target_altitude, (double)delta);
		}
		break;
	case ASCENDING:
		if (fabsf(current_z - _target_altitude) < HC_ALTITUDE_TOLERANCE) {
			_state = HOLDING_HIGH;
			PX4_INFO("HeightCommander: HOLDING_HIGH at %.2f", (double)current_z);
		}
		break;
	case HOLDING_HIGH:
		if (falling) {
			_target_altitude = _home_altitude;
			_state = DESCENDING;
			PX4_INFO("HeightCommander: DESCENDING to %.2f", (double)_home_altitude);
		}
		break;
	case DESCENDING:
		if (fabsf(current_z - _target_altitude) < HC_ALTITUDE_TOLERANCE) {
			PX4_INFO("HeightCommander: reached home altitude");
			resetToIdle();
			return;
		}
		break;
	}

	if (_state != IDLE) {
		publishOffboardSetpoint(_target_altitude);
		if (!in_offboard) {
			sendOffboardModeCommand();
		}
	}
}

void HeightCommander::Run()
{
	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup(desc);
		return;
	}
	perf_begin(_loop_perf);
	perf_count(_loop_interval_perf);

	if (_parameter_update_sub.updated()) {
		parameter_update_s param_update;
		_parameter_update_sub.copy(&param_update);
		updateParams();
	}
	updateStateMachine();
	perf_end(_loop_perf);
}

int HeightCommander::task_spawn(int argc, char *argv[])
{
	HeightCommander *instance = new HeightCommander();
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

int HeightCommander::print_status()
{
	PX4_INFO("State: %d, Home: %.2f, Target: %.2f, Delta: %.2f",
		(int)_state, (double)_home_altitude, (double)_target_altitude,
		(double)_param_hc_delta.get());
	perf_print_counter(_loop_perf);
	perf_print_counter(_loop_interval_perf);
	return 0;
}

int HeightCommander::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int HeightCommander::print_usage(const char *reason)
{
	if (reason) { PX4_WARN("%s\n", reason); }
	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Switch-triggered altitude change with RC stick override.
Toggle RC channel HIGH to ascend HC_DELTA meters.
Toggle LOW to descend back to original altitude.
RC stick movement cancels the command.
)DESCR_STR");
	PRINT_MODULE_USAGE_NAME("height_commander", "module");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return 0;
}

extern "C" __EXPORT int height_commander_main(int argc, char *argv[])
{
	return ModuleBase::main(HeightCommander::desc, argc, argv);
}
