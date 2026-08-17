#include "car.h"

#include "motor.h"
#include "servo.h"
#include "steering.h"

#define CAR_MAX_STEERING_ANGLE_DEG  90.0F
#define CAR_STEERING_CHANGE_EPSILON 0.01F

typedef enum
{
  CAR_STEERING_CENTER = 0,
  CAR_STEERING_TURN,
  CAR_STEERING_CRAB
} CarSteeringMode;

static CarSteeringMode car_steering_mode = CAR_STEERING_CENTER;
static float car_steering_angle_deg = 0.0F;
static uint8_t car_initialized = 0U;

static float car_clamp_steering_angle(float angle_deg)
{
  if (angle_deg > CAR_MAX_STEERING_ANGLE_DEG)
  {
    angle_deg = CAR_MAX_STEERING_ANGLE_DEG;
  }
  else if (angle_deg < -CAR_MAX_STEERING_ANGLE_DEG)
  {
    angle_deg = -CAR_MAX_STEERING_ANGLE_DEG;
  }

  return angle_deg;
}

static int car_angle_changed(float left, float right)
{
  float difference = left - right;

  if (difference < 0.0F)
  {
    difference = -difference;
  }

  return (difference > CAR_STEERING_CHANGE_EPSILON) ? 1 : 0;
}

static int16_t car_unsigned_to_signed(uint16_t speed)
{
  if (speed > 32767U)
  {
    speed = 32767U;
  }

  return (int16_t)speed;
}

static void car_set_steering_center(void)
{
  if ((car_initialized == 0U) ||
      (car_steering_mode != CAR_STEERING_CENTER) ||
      (car_angle_changed(car_steering_angle_deg, 0.0F) != 0))
  {
    steering_center();
    car_steering_mode = CAR_STEERING_CENTER;
    car_steering_angle_deg = 0.0F;
  }
}

static void car_set_steering(CarSteeringMode mode, float angle_deg)
{
  angle_deg = car_clamp_steering_angle(angle_deg);

  /* Avoid repeating steering.c's staggered GPIO updates every control cycle. */
  if ((car_steering_mode == mode) &&
      (car_angle_changed(car_steering_angle_deg, angle_deg) == 0))
  {
    return;
  }

  if (mode == CAR_STEERING_CRAB)
  {
    steering_crab(angle_deg);
  }
  else
  {
    steering_turn(angle_deg);
  }

  car_steering_mode = mode;
  car_steering_angle_deg = angle_deg;
}

void car_init(void)
{
  servo_init();
  motor_init();

  /* Power-on state is centered steering with every motor at zero PWM. */
  steering_center();
  motor_stop_all();

  car_steering_mode = CAR_STEERING_CENTER;
  car_steering_angle_deg = 0.0F;
  car_initialized = 1U;
}

void car_stop(void)
{
  /* Steering deliberately keeps its current angle. Call steering_center()
     separately when both stopped motors and centered steering are required. */
  motor_stop_all();
}

void car_forward(uint16_t speed)
{
  int16_t command = car_unsigned_to_signed(speed);

  car_set_steering_center();
  motor_set_all(command, command, command, command);
}

void car_backward(uint16_t speed)
{
  int16_t command = car_unsigned_to_signed(speed);

  car_set_steering_center();
  motor_set_all((int16_t)-command,
                (int16_t)-command,
                (int16_t)-command,
                (int16_t)-command);
}

void car_turn(float steering_angle, int16_t speed)
{
  car_set_steering(CAR_STEERING_TURN, steering_angle);
  motor_set_all(speed, speed, speed, speed);
}

void car_crab(float steering_angle, int16_t speed)
{
  car_set_steering(CAR_STEERING_CRAB, steering_angle);
  motor_set_all(speed, speed, speed, speed);
}
