#ifndef __PS2_USART_H
#define __PS2_USART_H

#include "stdint.h"
#define RX_BUF_SIZE 128
#define KEY_MID       0x0F    // 中位
#define KEY_UP        0x00    // 方向上 (上下(左摇杆上下)x5)
#define KEY_DOWN      0xFF    // 方向下 

#define KEY_LEFT      0x00    // 方向左 (左右(左摇杆左右)x4)
#define KEY_RIGHT     0xFF    // 方向右 

#define KEY_Y         0x1F     // Y键  (数字键和右摇杆都是x6)
#define KEY_A					0x4F     // A键
#define KEY_X         0x8F     // X键
#define KEY_B					0x2F     // B键
// 肩部按键 (x7 字节)
#define KEY_L1        0x01    // L1
#define KEY_L2        0x04    // L2
#define KEY_R1        0x02    // R1
#define KEY_R2        0x08		// R2
#define KEY_SELECTION 0x10

//遥控器数据结构体
typedef struct
{
    uint8_t frame_id;      // 帧号 0x01(有效)/0x02
    int16_t lx;            // 左摇杆X  中位0x80
    int16_t ly;            // 左摇杆Y  中位0x80
    
		//左边功能键
		uint8_t left_key;  //0:不按下  1:上   2:下   3:左   4:右
    // 右边功能键
	  uint8_t right_key; //0:不按下  1:上   2:下   3:左   4:右
        
    // 肩部按键
	  uint8_t top_key;   //0:不按下  1:左1  2:左2  3:右1  4:右2
    // 功能键
    uint8_t select  :1; // SELECT键
  
} PS2_HandleTypeDef;

extern uint8_t uart5_rx_finish;  // 接收完成标志
extern uint8_t uart5_rx_buf[RX_BUF_SIZE];
extern PS2_HandleTypeDef ps2;  //数据存储结构体
extern uint8_t data_flag;
extern uint8_t remote_stopped_flag ;          // 停止标志（1=遥控器无有效输入超过100ms）

void uasrt_rx_init(void);
void ps2_parse_data(uint8_t *str);



#endif

