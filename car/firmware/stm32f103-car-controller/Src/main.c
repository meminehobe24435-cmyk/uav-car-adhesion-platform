/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "car.h"
#include "motor.h"
#include "ps2_usart.h"
#include "steering.h"

#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
  uint8_t buttons;
  uint8_t right_key;
  uint8_t connected;
} Ps2Command;

typedef enum
{
  PS2_CAR_STOP = 0,
  PS2_CAR_FORWARD,
  PS2_CAR_BACKWARD,
  PS2_CAR_LEFT,
  PS2_CAR_RIGHT
} Ps2CarCommand;

/* USER CODE END PTD */

/* Private define ---------------------------------------------------------- --*/
/* USER CODE BEGIN PD */

#define PS2_CONTROL_PERIOD_MS          10U
#define PS2_DEBUG_PERIOD_MS            200U
#define PS2_CONNECTION_TIMEOUT_MS      500U
#define PS2_SPEED_GEAR_COUNT           3U
#define PS2_DEFAULT_GEAR_INDEX         0U
#define PS2_MAX_STEERING_ANGLE_DEG     90.0F

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static uint8_t ps2_line[RX_BUF_SIZE];
static Ps2Command ps2_command = {0};
static uint32_t ps2_last_valid_frame_ms = 0U;
static uint32_t ps2_last_debug_ms = 0U;
static uint16_t ps2_test_pwm = 0U;
static uint8_t ps2_run_latched = 0U;
static uint8_t ps2_l2_was_pressed = 0U;
static uint8_t ps2_start_was_pressed = 0U;
static uint8_t ps2_speed_gear_index = PS2_DEFAULT_GEAR_INDEX;
static Ps2CarCommand ps2_motion_command = PS2_CAR_STOP;
static const uint8_t ps2_speed_percent[PS2_SPEED_GEAR_COUNT] =
{
  30U,
  60U,
  80U
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void my_led_toggle(void);

static void ps2_command_clear(void)
{
  ps2_command.buttons = 0U;
  ps2_command.right_key = 0U;
  ps2_command.connected = 0U;
  ps2_run_latched = 0U;
  ps2_l2_was_pressed = 0U;
  ps2_start_was_pressed = 0U;
  ps2_motion_command = PS2_CAR_STOP;
}

static uint8_t ps2_copy_line(void)
{
  uint16_t index;

  if (uart5_rx_finish == 0U)
  {
    return 0U;
  }

  /* Copy the ISR-owned line atomically before parsing it in the main loop. */
  __disable_irq();
  for (index = 0U; index < (RX_BUF_SIZE - 1U); index++)
  {
    ps2_line[index] = uart5_rx_buf[index];
    if ((uart5_rx_buf[index] == '\0') ||
        (uart5_rx_buf[index] == '\r') ||
        (uart5_rx_buf[index] == '\n'))
    {
      ps2_line[index] = '\0';
      break;
    }
  }
  ps2_line[RX_BUF_SIZE - 1U] = '\0';
  uart5_rx_finish = 0U;
  __enable_irq();

  return 1U;
}

static void ps2_update(void)
{
  uint32_t now = HAL_GetTick();

  if (ps2_copy_line() != 0U)
  {
    /* A malformed line cannot reuse a previous valid frame identifier. */
    ps2.frame_id = 0U;
    ps2_parse_data(ps2_line);

    /* CH559 report 0x02 is the inactive logical joystick and is ignored. */
    if (ps2.frame_id == 0x01U)
    {
      ps2_command.buttons = ps2.buttons;
      ps2_command.right_key = ps2.right_key;
      ps2_command.connected = 1U;
      ps2_last_valid_frame_ms = now;
    }
  }

  if ((ps2_command.connected != 0U) &&
      ((uint32_t)(now - ps2_last_valid_frame_ms) >
       PS2_CONNECTION_TIMEOUT_MS))
  {
    car_stop();
    steering_center();
    ps2_command_clear();
  }
}

static Ps2CarCommand ps2_apply_command(void)
{
  uint8_t l2_pressed;
  uint8_t start_pressed;

  if (ps2_command.connected == 0U)
  {
    ps2_run_latched = 0U;
    ps2_l2_was_pressed = 0U;
    ps2_start_was_pressed = 0U;
    ps2_motion_command = PS2_CAR_STOP;
    car_stop();
    return PS2_CAR_STOP;
  }

  l2_pressed =
    ((ps2_command.buttons & PS2_BUTTON_L2) != 0U) ? 1U : 0U;
  start_pressed =
    ((ps2_command.buttons & PS2_BUTTON_START) != 0U) ? 1U : 0U;

  /* R1 always has priority and cancels the latched forward request. */
  if ((ps2_command.buttons & PS2_BUTTON_R1) != 0U)
  {
    ps2_run_latched = 0U;
    ps2_motion_command = PS2_CAR_STOP;
    ps2_l2_was_pressed = l2_pressed;
    ps2_start_was_pressed = start_pressed;
    car_stop();
    return PS2_CAR_STOP;
  }

  /* START stops immediately and centers once per physical press. */
  if (start_pressed != 0U)
  {
    ps2_run_latched = 0U;
    ps2_motion_command = PS2_CAR_STOP;
    ps2_l2_was_pressed = l2_pressed;
    car_stop();
    if (ps2_start_was_pressed == 0U)
    {
      steering_center();
    }
    ps2_start_was_pressed = 1U;
    return PS2_CAR_STOP;
  }
  ps2_start_was_pressed = 0U;

  /* Change exactly one gear on each L2 press, even when it is held down. */
  if ((l2_pressed != 0U) && (ps2_l2_was_pressed == 0U))
  {
    ps2_speed_gear_index++;
    if (ps2_speed_gear_index >= PS2_SPEED_GEAR_COUNT)
    {
      ps2_speed_gear_index = 0U;
    }
    ps2_test_pwm =
      motor_percent_to_pwm(ps2_speed_percent[ps2_speed_gear_index]);
  }
  ps2_l2_was_pressed = l2_pressed;

  /* Face-button commands use the existing, confirmed Byte5 parser. */
  if ((ps2_command.right_key == 1U) ||   /* Physical Y button: backward */
      (ps2_command.right_key == 2U))     /* Physical A button: backward */
  {
    ps2_motion_command = PS2_CAR_BACKWARD;
    ps2_run_latched = 1U;
  }
  else if (ps2_command.right_key == 3U)  /* Physical X button: left */
  {
    ps2_motion_command = PS2_CAR_LEFT;
    ps2_run_latched = 1U;
  }
  else if (ps2_command.right_key == 4U)  /* Physical B button: right */
  {
    ps2_motion_command = PS2_CAR_RIGHT;
    ps2_run_latched = 1U;
  }
  else if ((ps2_command.buttons & PS2_BUTTON_L1) != 0U)
  {
    ps2_motion_command = PS2_CAR_FORWARD;
    ps2_run_latched = 1U;
  }

  if (ps2_run_latched == 0U)
  {
    car_stop();
    return PS2_CAR_STOP;
  }

  switch (ps2_motion_command)
  {
    case PS2_CAR_FORWARD:
      car_forward(ps2_test_pwm);
      break;

    case PS2_CAR_BACKWARD:
      car_backward(ps2_test_pwm);
      break;

    case PS2_CAR_LEFT:
      car_turn(PS2_MAX_STEERING_ANGLE_DEG, (int16_t)ps2_test_pwm);
      break;

    case PS2_CAR_RIGHT:
      car_turn(-PS2_MAX_STEERING_ANGLE_DEG, (int16_t)ps2_test_pwm);
      break;

    default:
      ps2_run_latched = 0U;
      ps2_motion_command = PS2_CAR_STOP;
      car_stop();
      break;
  }

  return ps2_motion_command;
}

static const char *ps2_command_name(Ps2CarCommand command)
{
  static const char * const names[] =
  {
    "STOP",
    "FORWARD",
    "BACKWARD",
    "LEFT",
    "RIGHT"
  };

  if ((uint32_t)command >=
      (sizeof(names) / sizeof(names[0])))
  {
    return "STOP";
  }

  return names[command];
}

static void ps2_debug_update(Ps2CarCommand command)
{
  uint32_t now = HAL_GetTick();

  if ((uint32_t)(now - ps2_last_debug_ms) < PS2_DEBUG_PERIOD_MS)
  {
    return;
  }
  ps2_last_debug_ms = now;

  printf("PS2=%s BUTTONS=0x%02X FACE=%u RUN=%u GEAR=%u SPEED=%u%% PWM=%u TURN=90 CMD=%s\r\n",
         (ps2_command.connected != 0U) ? "CONNECTED" : "DISCONNECTED",
         ps2_command.buttons,
         ps2_command.right_key,
         ps2_run_latched,
         (uint32_t)ps2_speed_gear_index + 1U,
         ps2_speed_percent[ps2_speed_gear_index],
         ps2_test_pwm,
         ps2_command_name(command));
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  MX_USART2_UART_Init();
  MX_UART5_Init();
  /* USER CODE BEGIN 2 */

  /* car_init() reuses the verified servo and motor drivers. */
  car_init();
  car_stop();
  steering_center();

  ps2_command_clear();
  ps2_speed_gear_index = PS2_DEFAULT_GEAR_INDEX;
  ps2_test_pwm =
    motor_percent_to_pwm(ps2_speed_percent[ps2_speed_gear_index]);
  uasrt_rx_init();
  printf("PS2 NORMAL TURN90 READY PWM=%u FWD=L1 BACK=A/Y LEFT90=X RIGHT90=B STOP=R1 CENTER=START SPEED=L2 SELECT=UNUSED\r\n",
         ps2_test_pwm);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Ps2CarCommand command;

    ps2_update();
    command = ps2_apply_command();
    ps2_debug_update(command);
    HAL_Delay(PS2_CONTROL_PERIOD_MS);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV2;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void my_led_toggle(void)
{
	  HAL_Delay(100);
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_2);
		HAL_Delay(100);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
