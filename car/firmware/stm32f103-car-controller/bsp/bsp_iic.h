#ifndef __IIC_H
#define __IIC_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

// 引脚定义（与CubeMX配置一致）
#define IIC_SCL_PIN    GPIO_PIN_8
#define IIC_SDA_PIN    GPIO_PIN_9
#define IIC_GPIO_PORT  GPIOB

// 宏：SCL/SDA高低电平控制
#define IIC_SCL_HIGH() HAL_GPIO_WritePin(IIC_GPIO_PORT, IIC_SCL_PIN, GPIO_PIN_SET)
#define IIC_SCL_LOW()  HAL_GPIO_WritePin(IIC_GPIO_PORT, IIC_SCL_PIN, GPIO_PIN_RESET)
#define IIC_SDA_HIGH() HAL_GPIO_WritePin(IIC_GPIO_PORT, IIC_SDA_PIN, GPIO_PIN_SET)
#define IIC_SDA_LOW()  HAL_GPIO_WritePin(IIC_GPIO_PORT, IIC_SDA_PIN, GPIO_PIN_RESET)

// 读取SDA电平
#define IIC_READ_SDA() HAL_GPIO_ReadPin(IIC_GPIO_PORT, IIC_SDA_PIN)

// 方向控制（重配置SDA引脚模式）
void SDA_IN(void);
void SDA_OUT(void);

// 微秒延时（需用户提供，例如使用SysTick或定时器）
#define IIC_Delay() Delay_us(5)

// 对外接口函数
void IIC_Init(void);                        // 若CubeMX已初始化GPIO，可留空
uint8_t IIC_SendBytes(uint8_t Dev, uint8_t RegAddr, uint8_t Len, uint8_t *Data);
uint8_t IIC_ReadBytes(uint8_t Dev, uint8_t RegAddr, uint8_t Len, uint8_t *Data);

#endif
