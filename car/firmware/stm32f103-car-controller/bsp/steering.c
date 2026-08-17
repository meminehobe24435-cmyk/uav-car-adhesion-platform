#include "steering.h"

#include "servo.h"

#define STEERING_MAX_TEST_ANGLE_DEG       90.0F
#define STEERING_DIRECTION_TEST_ANGLE_DEG 10.0F
#define STEERING_OUTPUT_STAGGER_MS         100U
#define STEERING_ACTION_HOLD_MS           2000U
#define STEERING_START_HOLD_MS            3000U

static float steering_clamp_angle(float angle_deg)
{
  if (angle_deg > STEERING_MAX_TEST_ANGLE_DEG)
  {
    angle_deg = STEERING_MAX_TEST_ANGLE_DEG;
  }
  else if (angle_deg < -STEERING_MAX_TEST_ANGLE_DEG)
  {
    angle_deg = -STEERING_MAX_TEST_ANGLE_DEG;
  }

  return angle_deg;
}

static void steering_set_four_angles(float lf_deg,
                                     float rf_deg,
                                     float lr_deg,
                                     float rr_deg)
{
  servo_set_angle(SERVO_LF, steering_clamp_angle(lf_deg));
  HAL_Delay(STEERING_OUTPUT_STAGGER_MS);

  servo_set_angle(SERVO_RF, steering_clamp_angle(rf_deg));
  HAL_Delay(STEERING_OUTPUT_STAGGER_MS);

  servo_set_angle(SERVO_LR, steering_clamp_angle(lr_deg));
  HAL_Delay(STEERING_OUTPUT_STAGGER_MS);

  servo_set_angle(SERVO_RR, steering_clamp_angle(rr_deg));
}

static void steering_direction_test_one(ServoId id)
{
  /* The other three servos remain at their calibrated centers. */
  servo_center(id);
  HAL_Delay(STEERING_ACTION_HOLD_MS);

  servo_set_angle(id, STEERING_DIRECTION_TEST_ANGLE_DEG);
  HAL_Delay(STEERING_ACTION_HOLD_MS);

  servo_center(id);
  HAL_Delay(STEERING_ACTION_HOLD_MS);

  servo_set_angle(id, -STEERING_DIRECTION_TEST_ANGLE_DEG);
  HAL_Delay(STEERING_ACTION_HOLD_MS);

  servo_center(id);
  HAL_Delay(STEERING_ACTION_HOLD_MS);
}

void steering_center(void)
{
  servo_center_all();
}

void steering_test_angle(float angle_deg)
{
  angle_deg = steering_clamp_angle(angle_deg);
  steering_set_four_angles(angle_deg, angle_deg, angle_deg, angle_deg);
}

void steering_crab(float angle_deg)
{
  /* All wheels use the same vehicle-coordinate angle. */
  steering_test_angle(angle_deg);
}

void steering_turn(float angle_deg)
{
  angle_deg = steering_clamp_angle(angle_deg);

  /* Front axle follows angle; rear axle uses the opposite angle. */
  steering_set_four_angles(angle_deg,
                           angle_deg,
                           -angle_deg,
                           -angle_deg);
}

void steering_direction_calibration_test(void)
{
  steering_center();
  HAL_Delay(STEERING_START_HOLD_MS);

  while (1)
  {
    steering_direction_test_one(SERVO_LF);
    steering_direction_test_one(SERVO_RF);
    steering_direction_test_one(SERVO_LR);
    steering_direction_test_one(SERVO_RR);
  }
}

void steering_safe_angle_test(void)
{
  steering_center();
  HAL_Delay(STEERING_START_HOLD_MS);

  while (1)
  {
    steering_test_angle(0.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_test_angle(10.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_test_angle(0.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_test_angle(-10.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_test_angle(0.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
  }
}

void steering_safe_angle_20_test(void)
{
  steering_center();
  HAL_Delay(STEERING_START_HOLD_MS);

  /* Enable this test only after the +/-10 degree test is confirmed safe. */
  while (1)
  {
    steering_test_angle(0.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_test_angle(20.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_test_angle(0.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_test_angle(-20.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_test_angle(0.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
  }
}

void steering_basic_test(void)
{
  while (1)
  {
    steering_center();
    HAL_Delay(STEERING_START_HOLD_MS);

    steering_crab(10.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_center();
    HAL_Delay(STEERING_ACTION_HOLD_MS);

    steering_crab(-10.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_center();
    HAL_Delay(STEERING_ACTION_HOLD_MS);

    steering_turn(10.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_center();
    HAL_Delay(STEERING_ACTION_HOLD_MS);

    steering_turn(-10.0F);
    HAL_Delay(STEERING_ACTION_HOLD_MS);
    steering_center();
  }
}
