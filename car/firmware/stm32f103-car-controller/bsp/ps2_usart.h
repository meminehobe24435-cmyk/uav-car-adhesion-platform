#ifndef __PS2_USART_H
#define __PS2_USART_H

#include "stdint.h"
#define RX_BUF_SIZE 128
#define KEY_MID       0x0F    // 中位
#define KEY_UP        0x00    // 方向上 (上下(左摇杆上下)x5)
#define KEY_DOWN      0xFF    // 方向下

#define KEY_LEFT      0x00    // 方向左 (左右(左摇杆左右)x4)
#define KEY_RIGHT     0xFF    // 方向右

/* Byte5 face-button values confirmed with VID_0810/PID_0001. */
#define KEY_TRIANGLE  0x1F
#define KEY_CROSS     0x4F
#define KEY_SQUARE    0x8F
#define KEY_CIRCLE    0x2F

/* Byte6 is a bit mask; simultaneous buttons must not use equality tests. */
#define PS2_BUTTON_L1      0x01U
#define PS2_BUTTON_R1      0x02U
#define PS2_BUTTON_L2      0x04U
#define PS2_BUTTON_R2      0x08U
#define PS2_BUTTON_SELECT  0x10U
#define PS2_BUTTON_START   0x20U
#define PS2_BUTTON_L3      0x40U
#define PS2_BUTTON_R3      0x80U

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
    uint8_t buttons;   // Byte6 raw bit mask: L1/R1/L2/R2/SELECT/START/L3/R3
    // 功能键
    uint8_t select  :1; // SELECT键
  
} PS2_HandleTypeDef;

extern volatile uint8_t uart5_rx_finish;  // 接收完成标志
extern uint8_t uart5_rx_buf[RX_BUF_SIZE];
extern PS2_HandleTypeDef ps2;  //数据存储结构体
extern uint8_t data_flag;
extern uint8_t remote_stopped_flag ;          // 停止标志（1=遥控器无有效输入超过100ms）

void uasrt_rx_init(void);
void ps2_parse_data(uint8_t *str);



#endif

