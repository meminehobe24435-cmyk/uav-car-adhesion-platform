#include "usart.h"
#include "stm32f1xx_hal.h"

#include "vofa.h"
#include "chassis_task.h"
#include "pid.h"
#define MAX_CHANNEL 8

#define BYTE0(dwTemp)       (*(char *)(dwTemp))
#define BYTE1(dwTemp)       (*((char *)(dwTemp) + 1))
#define BYTE2(dwTemp)       (*((char *)(dwTemp) + 2))
#define BYTE3(dwTemp)       (*((char *)(dwTemp) + 3))


float *UserData[MAX_CHANNEL]={0};//only transmit float
unsigned char Data_Number = 8;

void Upper_Computer_Init(float* addr)
{
    if(Data_Number < MAX_CHANNEL)UserData[Data_Number++]=addr;
}

unsigned char data_to_send[4*MAX_CHANNEL+4] = {0};
void Upper_Computer_Show_Wave(void)
{
	  unsigned char cnt = 0;
    UserData[0]=&a;
    UserData[1]=&b;
    UserData[2]=&c;
	  UserData[3]=&d;
	  UserData[4]=&e;
	  UserData[5]=&f;
    UserData[6]=&g;
//	  UserData[7]=&distance1;
		for(int i=0;i<Data_Number;i++)
		{
			data_to_send[cnt++] = BYTE0(UserData[i]);
			data_to_send[cnt++] = BYTE1(UserData[i]);
			data_to_send[cnt++] = BYTE2(UserData[i]);
			data_to_send[cnt++] = BYTE3(UserData[i]);
		}

		data_to_send[cnt++] = 0x00;
		data_to_send[cnt++] = 0x00;
		data_to_send[cnt++] = 0x80;
		data_to_send[cnt++] = 0x7F;
		 for(int i=0;i<4*MAX_CHANNEL+4;i++)
		 {   
			 
			 while (HAL_UART_GetState(&huart2) != HAL_UART_STATE_READY)
			{
					continue;
			}
			// 发送单个字节
			HAL_UART_Transmit(&huart2, &data_to_send[i], 1, 50);
		 }
		 
	//----------------end----------------------
}


