#include "com_debug.h"

/**
  * 函    数：串口重定向
  * 参    数：无
  * 返 回 值：无
  */
int fputc(int c, FILE *file)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&c, 1, 50);
    return c;
}


