#include "bsp_key.h"
#include "stm32f1xx_hal.h"

//按键扫描函数，返回的是整个按键结构体
KEY_STATUS Key_Scan(void)
{
   KEY_STATUS states;
   states.key_up   = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4)   ? 1 : 0; 
//   states.key_left = DL_GPIO_readPins(GPIO_KEY_PIN_LEFT_PORT, GPIO_KEY_PIN_LEFT_PIN) ? 1 : 0;
//   states.key_right= DL_GPIO_readPins(GPIO_KEY_PIN_RIGHT_PORT,GPIO_KEY_PIN_RIGHT_PIN)? 1 : 0;
//   states.key_down = DL_GPIO_readPins(GPIO_KEY_PIN_DOWN_PORT, GPIO_KEY_PIN_DOWN_PIN) ? 1 : 0;
//   states.key_mid  = DL_GPIO_readPins(GPIO_KEY_PIN_MID_PORT,  GPIO_KEY_PIN_MID_PIN)  ? 1 : 0;
   return states;
}




