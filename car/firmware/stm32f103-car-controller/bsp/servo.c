#include "servo.h"

#include "main.h"
#include "tim.h"

#define SERVO_START_STAGGER_MS  100U
#define SERVO_CAL_STEP_US        50U
#define SERVO_CAL_STEP_DELAY_MS 1000U
#define SERVO_CAL_START_DELAY_MS 3000U
#define SERVO_ANGLE_LIMIT_DEG    90.0F

#define SERVO_LR_GPIO_PORT      GPIOC
#define SERVO_LR_GPIO_PIN       GPIO_PIN_5
#define SERVO_RR_GPIO_PORT      GPIOB
#define SERVO_RR_GPIO_PIN       GPIO_PIN_0

/*
 * Four-servo calibration parameters.
 * Adjust center_us to straighten each wheel and direction to reverse the
 * meaning of positive angles. Keep min_us/max_us conservative during setup.
 */
ServoConfig servo_config[SERVO_COUNT] =
{
  {1500U, 1400U, 1600U, 1}, /* SERVO_LF: J9  / PA0 / TIM5_CH1 */
  {1500U, 1400U, 1600U, 1}, /* SERVO_RF: J11 / PA1 / TIM5_CH2 */
  {1500U, 1400U, 1600U, 1}, /* SERVO_LR: J12 / PC5 / software PWM */
  {1500U, 1400U, 1600U, 1}  /* SERVO_RR: J14 / PB0 / software PWM */
};

static volatile uint16_t servo_pending_us[SERVO_COUNT] =
{
  1500U, 1500U, 1500U, 1500U
};

static volatile uint8_t servo_lr_enabled = 0U;
static volatile uint8_t servo_rr_enabled = 0U;
static uint8_t servo_initialized = 0U;

static void servo_check_hal_status(HAL_StatusTypeDef status)
{
  if (status != HAL_OK)
  {
    Error_Handler();
  }
}

static uint16_t servo_clamp_us(ServoId id, uint16_t us)
{
  const ServoConfig *config = &servo_config[id];

  if (us < config->min_us)
  {
    us = config->min_us;
  }
  else if (us > config->max_us)
  {
    us = config->max_us;
  }

  return us;
}

static void servo_calibration_one(ServoId id)
{
  uint16_t center_us = servo_config[id].center_us;
  uint16_t lower_us;
  uint16_t upper_us;

  lower_us = (center_us >= SERVO_CAL_STEP_US) ?
             (uint16_t)(center_us - SERVO_CAL_STEP_US) : 0U;
  upper_us = (center_us <= (uint16_t)(0xFFFFU - SERVO_CAL_STEP_US)) ?
             (uint16_t)(center_us + SERVO_CAL_STEP_US) : 0xFFFFU;

  servo_center(id);
  HAL_Delay(SERVO_CAL_STEP_DELAY_MS);
  servo_set_us(id, lower_us);
  HAL_Delay(SERVO_CAL_STEP_DELAY_MS);
  servo_center(id);
  HAL_Delay(SERVO_CAL_STEP_DELAY_MS);
  servo_set_us(id, upper_us);
  HAL_Delay(SERVO_CAL_STEP_DELAY_MS);
  servo_center(id);
  HAL_Delay(SERVO_CAL_STEP_DELAY_MS);
}

static void servo_gpio_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* RF: PA1 / TIM5_CH2 hardware PWM. */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  HAL_GPIO_WritePin(SERVO_LR_GPIO_PORT, SERVO_LR_GPIO_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = SERVO_LR_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SERVO_LR_GPIO_PORT, &GPIO_InitStruct);

  HAL_GPIO_WritePin(SERVO_RR_GPIO_PORT, SERVO_RR_GPIO_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = SERVO_RR_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SERVO_RR_GPIO_PORT, &GPIO_InitStruct);
}

static void servo_tim5_channels_init(void)
{
  TIM_OC_InitTypeDef channel_config = {0};

  channel_config.OCMode = TIM_OCMODE_PWM1;
  channel_config.Pulse = servo_pending_us[SERVO_RF];
  channel_config.OCPolarity = TIM_OCPOLARITY_HIGH;
  channel_config.OCFastMode = TIM_OCFAST_DISABLE;
  servo_check_hal_status(HAL_TIM_PWM_ConfigChannel(&htim5,
                                                    &channel_config,
                                                    TIM_CHANNEL_2));

  /* CH3/CH4 are internal compare time markers only; their pins stay disabled. */
  channel_config.OCMode = TIM_OCMODE_TIMING;
  channel_config.Pulse = servo_pending_us[SERVO_LR];
  servo_check_hal_status(HAL_TIM_OC_ConfigChannel(&htim5,
                                                   &channel_config,
                                                   TIM_CHANNEL_3));

  channel_config.Pulse = servo_pending_us[SERVO_RR];
  servo_check_hal_status(HAL_TIM_OC_ConfigChannel(&htim5,
                                                   &channel_config,
                                                   TIM_CHANNEL_4));

  __HAL_TIM_ENABLE_OCxPRELOAD(&htim5, TIM_CHANNEL_1);
  __HAL_TIM_ENABLE_OCxPRELOAD(&htim5, TIM_CHANNEL_2);
  __HAL_TIM_DISABLE_OCxPRELOAD(&htim5, TIM_CHANNEL_3);
  __HAL_TIM_DISABLE_OCxPRELOAD(&htim5, TIM_CHANNEL_4);

  __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1,
                        servo_pending_us[SERVO_LF]);
  __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_2,
                        servo_pending_us[SERVO_RF]);
  __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_3,
                        servo_pending_us[SERVO_LR]);
  __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_4,
                        servo_pending_us[SERVO_RR]);

  __HAL_TIM_SET_COUNTER(&htim5, 0U);
  htim5.Instance->EGR = TIM_EGR_UG;
  __HAL_TIM_CLEAR_FLAG(&htim5,
                       TIM_FLAG_UPDATE | TIM_FLAG_CC3 | TIM_FLAG_CC4);
}

void servo_init(void)
{
  if (servo_initialized != 0U)
  {
    return;
  }

  /* Load edited calibration centers before any PWM output is enabled. */
  servo_pending_us[SERVO_LF] = servo_clamp_us(SERVO_LF,
                                               servo_config[SERVO_LF].center_us);
  servo_pending_us[SERVO_RF] = servo_clamp_us(SERVO_RF,
                                               servo_config[SERVO_RF].center_us);
  servo_pending_us[SERVO_LR] = servo_clamp_us(SERVO_LR,
                                               servo_config[SERVO_LR].center_us);
  servo_pending_us[SERVO_RR] = servo_clamp_us(SERVO_RR,
                                               servo_config[SERVO_RR].center_us);

  servo_gpio_init();
  servo_tim5_channels_init();

  HAL_NVIC_SetPriority(TIM5_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(TIM5_IRQn);

  /* CH3/CH4 compare interrupts do not enable their physical output pins. */
  __HAL_TIM_ENABLE_IT(&htim5, TIM_IT_UPDATE | TIM_IT_CC3 | TIM_IT_CC4);

  /* Stagger the four outputs by 100 ms to reduce startup current steps. */
  servo_check_hal_status(HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1));
  HAL_Delay(SERVO_START_STAGGER_MS);

  servo_check_hal_status(HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_2));
  HAL_Delay(SERVO_START_STAGGER_MS);

  servo_lr_enabled = 1U;
  HAL_Delay(SERVO_START_STAGGER_MS);

  servo_rr_enabled = 1U;
  servo_initialized = 1U;
}

void servo_set_us(ServoId id, uint16_t us)
{
  uint16_t output_us;

  if ((uint32_t)id >= (uint32_t)SERVO_COUNT)
  {
    return;
  }

  output_us = servo_clamp_us(id, us);
  servo_pending_us[id] = output_us;

  if (servo_initialized == 0U)
  {
    return;
  }

  if (id == SERVO_LF)
  {
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, output_us);
  }
  else if (id == SERVO_RF)
  {
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_2, output_us);
  }
}

void servo_center(ServoId id)
{
  if ((uint32_t)id < (uint32_t)SERVO_COUNT)
  {
    servo_set_us(id, servo_config[id].center_us);
  }
}

void servo_center_all(void)
{
  servo_set_all(servo_config[SERVO_LF].center_us,
                servo_config[SERVO_RF].center_us,
                servo_config[SERVO_LR].center_us,
                servo_config[SERVO_RR].center_us);
}

void servo_set_center(ServoId id, uint16_t center_us)
{
  if ((uint32_t)id >= (uint32_t)SERVO_COUNT)
  {
    return;
  }

  servo_config[id].center_us = servo_clamp_us(id, center_us);
  servo_center(id);
}

void servo_set_direction(ServoId id, int8_t direction)
{
  if (((uint32_t)id >= (uint32_t)SERVO_COUNT) ||
      ((direction != 1) && (direction != -1)))
  {
    return;
  }

  servo_config[id].direction = direction;
}

void servo_set_angle(ServoId id, float angle_deg)
{
  const ServoConfig *config;
  float directed_angle;
  float output_us;

  if ((uint32_t)id >= (uint32_t)SERVO_COUNT)
  {
    return;
  }

  config = &servo_config[id];

  if (angle_deg > SERVO_ANGLE_LIMIT_DEG)
  {
    angle_deg = SERVO_ANGLE_LIMIT_DEG;
  }
  else if (angle_deg < -SERVO_ANGLE_LIMIT_DEG)
  {
    angle_deg = -SERVO_ANGLE_LIMIT_DEG;
  }

  directed_angle = angle_deg * (float)config->direction;
  if (directed_angle >= 0.0F)
  {
    output_us = (float)config->center_us +
                (directed_angle / SERVO_ANGLE_LIMIT_DEG) *
                (float)(config->max_us - config->center_us);
  }
  else
  {
    output_us = (float)config->center_us +
                (directed_angle / SERVO_ANGLE_LIMIT_DEG) *
                (float)(config->center_us - config->min_us);
  }

  servo_set_us(id, (uint16_t)(output_us + 0.5F));
}

void servo_set_all(uint16_t lf,
                   uint16_t rf,
                   uint16_t lr,
                   uint16_t rr)
{
  servo_set_us(SERVO_LF, lf);
  HAL_Delay(SERVO_START_STAGGER_MS);

  servo_set_us(SERVO_RF, rf);
  HAL_Delay(SERVO_START_STAGGER_MS);

  servo_set_us(SERVO_LR, lr);
  HAL_Delay(SERVO_START_STAGGER_MS);

  servo_set_us(SERVO_RR, rr);
}

void servo_calibration_test(void)
{
  servo_center_all();
  HAL_Delay(SERVO_CAL_START_DELAY_MS);

  while (1)
  {
    servo_calibration_one(SERVO_LF);
    servo_calibration_one(SERVO_RF);
    servo_calibration_one(SERVO_LR);
    servo_calibration_one(SERVO_RR);
  }
}

void servo_tim5_irq_handler(void)
{
  if ((__HAL_TIM_GET_FLAG(&htim5, TIM_FLAG_UPDATE) != RESET) &&
      (__HAL_TIM_GET_IT_SOURCE(&htim5, TIM_IT_UPDATE) != RESET))
  {
    __HAL_TIM_CLEAR_IT(&htim5, TIM_IT_UPDATE);

    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_3,
                          servo_pending_us[SERVO_LR]);
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_4,
                          servo_pending_us[SERVO_RR]);

    if (servo_lr_enabled != 0U)
    {
      HAL_GPIO_WritePin(SERVO_LR_GPIO_PORT,
                        SERVO_LR_GPIO_PIN,
                        GPIO_PIN_SET);
    }

    if (servo_rr_enabled != 0U)
    {
      HAL_GPIO_WritePin(SERVO_RR_GPIO_PORT,
                        SERVO_RR_GPIO_PIN,
                        GPIO_PIN_SET);
    }
  }

  if ((__HAL_TIM_GET_FLAG(&htim5, TIM_FLAG_CC3) != RESET) &&
      (__HAL_TIM_GET_IT_SOURCE(&htim5, TIM_IT_CC3) != RESET))
  {
    __HAL_TIM_CLEAR_IT(&htim5, TIM_IT_CC3);
    HAL_GPIO_WritePin(SERVO_LR_GPIO_PORT,
                      SERVO_LR_GPIO_PIN,
                      GPIO_PIN_RESET);
  }

  if ((__HAL_TIM_GET_FLAG(&htim5, TIM_FLAG_CC4) != RESET) &&
      (__HAL_TIM_GET_IT_SOURCE(&htim5, TIM_IT_CC4) != RESET))
  {
    __HAL_TIM_CLEAR_IT(&htim5, TIM_IT_CC4);
    HAL_GPIO_WritePin(SERVO_RR_GPIO_PORT,
                      SERVO_RR_GPIO_PIN,
                      GPIO_PIN_RESET);
  }
}
