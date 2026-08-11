#ifndef __COM_DEBUG_H
#define __COM_DEBUG_H

#include "usart.h"
#include "string.h"
#include "stdio.h"
 //0是vofa调试 
 //5是ps2遥控器串口调试  
 //2是imu串口调试 
 //3是雷达数据调试 
 //其他数字是无调试
#define DEBUG_TEST  5          

#define DEBUG 1

#if(DEBUG == 1)

#define FINENAME strrchr(__FILE__, '\\') ? (strrchr(__FILE__, '\\') + 1) : __FILE__

#define debug_printf(format, ...) printf("[%15s:%4d] -- " format, FINENAME, __LINE__ , ##__VA_ARGS__)
#define debug_printfln(format, ...) printf("[%15s:%4d] -- " format "\r\n", FINENAME, __LINE__ , ##__VA_ARGS__)
#else

#define debug_start() 
#define debug_printf(format, ...) 
#define debug_printfln(format, ...) 

#endif

void Com_Debug_Start(void);
#endif

