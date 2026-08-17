#include "rc_control.h"

#include "car.h"
#include "main.h"

#define RC_INPUT_MIN       (-1000)
#define RC_INPUT_MAX       1000
#define RC_LINK_TIMEOUT_MS 500U

static RcCommand rc_command;
static uint32_t rc_last_frame_tick = 0U;
static uint8_t rc_has_received_frame = 0U;

static int16_t rc_clamp_input(int16_t value)
{
  if (value > RC_INPUT_MAX)
  {
    value = RC_INPUT_MAX;
  }
  else if (value < RC_INPUT_MIN)
  {
    value = RC_INPUT_MIN;
  }

  return value;
}

static void rc_enter_failsafe(void)
{
  rc_command.throttle = 0;
  rc_command.steering = 0;
  rc_command.mode = 0U;
  rc_command.enable = 0U;
  rc_command.connected = 0U;
  car_stop();
}

__weak uint8_t rc_backend_read(RcCommand *command)
{
  (void)command;
  return 0U;
}

void rc_init(void)
{
  rc_command.throttle = 0;
  rc_command.steering = 0;
  rc_command.mode = 0U;
  rc_command.enable = 0U;
  rc_command.connected = 0U;
  rc_last_frame_tick = HAL_GetTick();
  rc_has_received_frame = 0U;

  /* No protocol backend is active yet, so power-on must remain stopped. */
  car_stop();
}

void rc_update(void)
{
  RcCommand new_command;
  uint32_t now = HAL_GetTick();

  new_command.throttle = 0;
  new_command.steering = 0;
  new_command.mode = 0U;
  new_command.enable = 0U;
  new_command.connected = 0U;

  if (rc_backend_read(&new_command) != 0U)
  {
    new_command.throttle = rc_clamp_input(new_command.throttle);
    new_command.steering = rc_clamp_input(new_command.steering);
    new_command.mode = (new_command.mode == 1U) ? 1U : 0U;
    new_command.enable = (new_command.enable != 0U) ? 1U : 0U;
    new_command.connected = (new_command.connected != 0U) ? 1U : 0U;

    rc_command = new_command;
    if (new_command.connected != 0U)
    {
      rc_last_frame_tick = now;
      rc_has_received_frame = 1U;
    }
    else
    {
      rc_has_received_frame = 0U;
    }
  }

  if ((rc_has_received_frame == 0U) ||
      (rc_command.connected == 0U) ||
      ((uint32_t)(now - rc_last_frame_tick) > RC_LINK_TIMEOUT_MS))
  {
    rc_enter_failsafe();
    rc_has_received_frame = 0U;
    return;
  }

  if (rc_command.enable == 0U)
  {
    rc_command.throttle = 0;
    car_stop();
  }
}

RcCommand rc_get_command(void)
{
  return rc_command;
}
