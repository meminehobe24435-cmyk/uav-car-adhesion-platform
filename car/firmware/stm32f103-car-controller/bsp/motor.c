#include "motor.h"

#include "main.h"
#include "tim.h"

#include "math.h"

#define MOTOR_FIRST_LIMIT_PERCENT  80U
#define MOTOR_FIRST_TEST_PERCENT   20U
#define MOTOR_TEST_RUN_MS              2000U
#define MOTOR_TEST_INITIAL_STOP_MS     2000U
#define MOTOR_TEST_DIRECTION_STOP_MS   1000U
#define MOTOR_TEST_BETWEEN_MOTORS_MS   2000U
#define MOTOR_TEST_LOOP_END_MS         3000U

typedef struct
{
  uint32_t tim_channel;
  GPIO_TypeDef *in1_port;
  uint16_t in1_pin;
  GPIO_TypeDef *in2_port;
  uint16_t in2_pin;
} MotorHardware;

/*
 * Central motor calibration table.
 * pwm_max=80 is 80% because TIM2 ARR=99 gives 100 counts per PWM period.
 * Change only direction after physical forward direction is observed.
 */
MotorConfig motor_config[MOTOR_COUNT] =
{
  {1, 80U}, /* MOTOR_1: J1 / TB6612_1 channel A / TIM2_CH1 */
  {1, 80U}, /* MOTOR_2: J2 / TB6612_1 channel B / TIM2_CH2 */
  {1, 80U}, /* MOTOR_3: J3 / TB6612_2 channel A / TIM2_CH4 */
  {-1, 80U} /* MOTOR_4: J4 / PA0 wheel; reversed for vehicle-forward */
};

static const MotorHardware motor_hardware[MOTOR_COUNT] =
{
  {TIM_CHANNEL_1, GPIOC, GPIO_PIN_13, GPIOC, GPIO_PIN_14},
  {TIM_CHANNEL_2, GPIOC, GPIO_PIN_0,  GPIOC, GPIO_PIN_1},
  {TIM_CHANNEL_4, GPIOB, GPIO_PIN_13, GPIOB, GPIO_PIN_12},
  {TIM_CHANNEL_3, GPIOC, GPIO_PIN_9,  GPIOC, GPIO_PIN_8}
};

static uint8_t motor_initialized = 0U;

static void motor_check_hal_status(HAL_StatusTypeDef status)
{
  if (status != HAL_OK)
  {
    Error_Handler();
  }
}

static void motor_set_standby(GPIO_PinState state)
{
  /* TB6612_1_STBY=PC15, TB6612_2_STBY=PB14. */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, state);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, state);
}

static void motor_apply_output(MotorId id, int32_t signed_pwm)
{
  const MotorHardware *hardware = &motor_hardware[id];
  uint16_t magnitude;

  /* Remove PWM before changing direction pins. */
  __HAL_TIM_SET_COMPARE(&htim2, hardware->tim_channel, 0U);

  if (signed_pwm > 0)
  {
    HAL_GPIO_WritePin(hardware->in1_port, hardware->in1_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(hardware->in2_port, hardware->in2_pin, GPIO_PIN_RESET);
    magnitude = (uint16_t)signed_pwm;
  }
  else if (signed_pwm < 0)
  {
    HAL_GPIO_WritePin(hardware->in1_port, hardware->in1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hardware->in2_port, hardware->in2_pin, GPIO_PIN_SET);
    magnitude = (uint16_t)(-signed_pwm);
  }
  else
  {
    HAL_GPIO_WritePin(hardware->in1_port, hardware->in1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hardware->in2_port, hardware->in2_pin, GPIO_PIN_RESET);
    magnitude = 0U;
  }

  __HAL_TIM_SET_COMPARE(&htim2, hardware->tim_channel, magnitude);
}

static int16_t motor_unsigned_to_signed(uint16_t pwm)
{
  if (pwm > 32767U)
  {
    pwm = 32767U;
  }

  return (int16_t)pwm;
}

static void motor_four_test_one(MotorId id,
                                uint16_t pwm,
                                uint32_t final_stop_ms)
{
  /* Reassert that every non-selected motor is stopped before each step. */
  motor_stop_all();

  motor_forward(id, pwm);
  HAL_Delay(MOTOR_TEST_RUN_MS);
  motor_stop_all();
  HAL_Delay(MOTOR_TEST_DIRECTION_STOP_MS);

  motor_reverse(id, pwm);
  HAL_Delay(MOTOR_TEST_RUN_MS);
  motor_stop_all();
  HAL_Delay(final_stop_ms);
}

uint16_t motor_get_pwm_full_scale(void)
{
  uint32_t full_scale = __HAL_TIM_GET_AUTORELOAD(&htim2) + 1U;

  if (full_scale > 65535U)
  {
    full_scale = 65535U;
  }

  return (uint16_t)full_scale;
}

uint16_t motor_percent_to_pwm(uint16_t percent)
{
  uint32_t pwm;

  if (percent > 100U)
  {
    percent = 100U;
  }

  pwm = ((uint32_t)motor_get_pwm_full_scale() * (uint32_t)percent) / 100U;
  return (uint16_t)pwm;
}

void motor_init(void)
{
  uint16_t first_limit;
  uint32_t index;

  if (motor_initialized != 0U)
  {
    return;
  }

  /* Keep both drivers disabled while directions and zero duty are prepared. */
  motor_set_standby(GPIO_PIN_RESET);
  for (index = 0U; index < (uint32_t)MOTOR_COUNT; index++)
  {
    motor_apply_output((MotorId)index, 0);
  }

  motor_check_hal_status(HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1));
  motor_check_hal_status(HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2));
  motor_check_hal_status(HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3));
  motor_check_hal_status(HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4));

  /* Enforce the configured 80% ceiling even if a table value is too large. */
  first_limit = motor_percent_to_pwm(MOTOR_FIRST_LIMIT_PERCENT);
  for (index = 0U; index < (uint32_t)MOTOR_COUNT; index++)
  {
    if (motor_config[index].pwm_max > first_limit)
    {
      motor_config[index].pwm_max = first_limit;
    }
  }

  motor_set_standby(GPIO_PIN_SET);
  motor_initialized = 1U;
}

void motor_set_pwm(MotorId id, int16_t pwm)
{
  int32_t signed_pwm;
  int32_t magnitude;
  int32_t direction;
  uint16_t full_scale;
  uint16_t output_limit;

  if (((uint32_t)id >= (uint32_t)MOTOR_COUNT) ||
      (motor_initialized == 0U))
  {
    return;
  }

  direction = (motor_config[id].direction == -1) ? -1 : 1;
  signed_pwm = (int32_t)pwm * direction;
  magnitude = (signed_pwm < 0) ? -signed_pwm : signed_pwm;

  full_scale = motor_get_pwm_full_scale();
  output_limit = motor_config[id].pwm_max;
  if (output_limit > full_scale)
  {
    output_limit = full_scale;
  }

  if (magnitude > (int32_t)output_limit)
  {
    magnitude = (int32_t)output_limit;
    signed_pwm = (signed_pwm < 0) ? -magnitude : magnitude;
  }

  motor_apply_output(id, signed_pwm);
}

void motor_forward(MotorId id, uint16_t pwm)
{
  motor_set_pwm(id, motor_unsigned_to_signed(pwm));
}

void motor_reverse(MotorId id, uint16_t pwm)
{
  motor_set_pwm(id, (int16_t)-motor_unsigned_to_signed(pwm));
}

void motor_stop(MotorId id)
{
  motor_set_pwm(id, 0);
}

void motor_stop_all(void)
{
  motor_stop(MOTOR_1);
  motor_stop(MOTOR_2);
  motor_stop(MOTOR_3);
  motor_stop(MOTOR_4);
}

void motor_set_all(int16_t motor_1,
                   int16_t motor_2,
                   int16_t motor_3,
                   int16_t motor_4)
{
  motor_set_pwm(MOTOR_1, motor_1);
  motor_set_pwm(MOTOR_2, motor_2);
  motor_set_pwm(MOTOR_3, motor_3);
  motor_set_pwm(MOTOR_4, motor_4);
}

void motor_single_test(void)
{
  uint16_t test_pwm = motor_percent_to_pwm(MOTOR_FIRST_TEST_PERCENT);

  while (1)
  {
    motor_stop_all();
    HAL_Delay(MOTOR_TEST_INITIAL_STOP_MS);

    motor_forward(MOTOR_1, test_pwm);
    HAL_Delay(MOTOR_TEST_RUN_MS);

    motor_stop_all();
    HAL_Delay(MOTOR_TEST_BETWEEN_MOTORS_MS);

    motor_reverse(MOTOR_1, test_pwm);
    HAL_Delay(MOTOR_TEST_RUN_MS);

    motor_stop_all();
    HAL_Delay(MOTOR_TEST_LOOP_END_MS);
  }
}

void motor_four_test(void)
{
  uint16_t test_pwm = motor_percent_to_pwm(MOTOR_FIRST_TEST_PERCENT);

  while (1)
  {
    motor_stop_all();
    HAL_Delay(MOTOR_TEST_INITIAL_STOP_MS);

    motor_four_test_one(MOTOR_1, test_pwm, MOTOR_TEST_BETWEEN_MOTORS_MS);
    motor_four_test_one(MOTOR_2, test_pwm, MOTOR_TEST_BETWEEN_MOTORS_MS);
    motor_four_test_one(MOTOR_3, test_pwm, MOTOR_TEST_BETWEEN_MOTORS_MS);
    motor_four_test_one(MOTOR_4, test_pwm, MOTOR_TEST_LOOP_END_MS);
  }
}

void motor_ctrol(Chassis_TypeDef *chassis)
{
  int stop;

  if (chassis == 0)
  {
    motor_stop_all();
    return;
  }

  stop = (fabsf(chassis->vx) < 0.01f &&
          fabsf(chassis->vy) < 0.01f &&
          fabsf(chassis->vz) < 0.01f);

  if (stop != 0)
  {
    motor_stop_all();
    return;
  }

  /* Preserve the original chassis-array to physical-connector mapping. */
  motor_set_pwm(MOTOR_3, chassis->motor[0].speed);
  motor_set_pwm(MOTOR_1, chassis->motor[1].speed);
  motor_set_pwm(MOTOR_4, chassis->motor[2].speed);
  motor_set_pwm(MOTOR_2, chassis->motor[3].speed);
}
