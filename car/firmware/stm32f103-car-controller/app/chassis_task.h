#ifndef __CHASSIS_TASK
#define __CHASSIS_TASK

#include "pid.h"
#include "motor.h"
#include "stm32f1xx_hal.h"

#define LED_ON()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET)
#define LED_OFF()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET)
// 底盘几何参数
#define WHEEL_BASE_SUM    0.168f   // L+W = 边长 = 0.2 m
#define MAX_WHEEL_SPEED   1.08f   // 轮子最大线速度 1 m/s
extern float a;
extern float b;
extern float c;
extern float d;
extern float e;
extern float f;
extern float g;

void chassic_task(void const * argument);
void motor_pid_test(void);


#endif

