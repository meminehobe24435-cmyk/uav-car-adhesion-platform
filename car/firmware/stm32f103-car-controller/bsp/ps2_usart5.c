//简单中断接收 后续可扩展dma+空闲中断
#include "ps2_usart.h"
#include "usart.h"
#include "stm32f1xx_hal.h"
#include "calc_fun.h"
#include "stdio.h"
#include "chassis_task.h"
#include "com_debug.h"

uint8_t rx_data;
uint8_t uart5_rx_buf[RX_BUF_SIZE];
uint16_t uart5_rx_cnt = 0;
volatile uint8_t uart5_rx_finish = 0;  // 接收完成标志（核心！）

PS2_HandleTypeDef ps2 = {0};  //数据存储结构体
extern Chassis_TypeDef chassis; 
uint8_t remote_stopped_flag = 0;          // 停止标志（1=遥控器无有效输入超过100ms）
static uint32_t last_nonzero_tick = 0;    // 最后一次收到非零数据的系统节拍
/**
  * 函    数：串口初始化
  * 参    数：无
  * 返 回 值：无
  */
void uasrt_rx_init(void)
{
    HAL_UART_Receive_IT(&huart5,&rx_data,1);
}
void Uart5_SendByte(uint8_t Byte)
{
	while(huart5.gState != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart5, &Byte, 1, 10);
}

/**
  * 函    数：串口中断回调函数
  * 参    数：无
  * 返 回 值：无
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == UART5)
    {
        // 仅存数据
        if(uart5_rx_cnt < RX_BUF_SIZE-1)
        {
            uart5_rx_buf[uart5_rx_cnt++] = rx_data;
        }

        // 一帧接收完成
        if(rx_data == '\n' || uart5_rx_cnt >= 64)
        {
            uart5_rx_buf[uart5_rx_cnt] = '\0';
            uart5_rx_finish = 1;  // 标记完成
            uart5_rx_cnt = 0;
        }

        // 重新开启接收
        HAL_UART_Receive_IT(&huart5, &rx_data, 1);
    }
}

#define PS2_MAP     128
// 原始值 x (0-255) → 映射成 -128-128
int map_joystick(int x)
{
    float val = (x - 128); 
                            
    return (int)val;
}
//死区限幅
int joystick_deadzone(int val)
{
    if (abs_int(val) < 5)
        return 0;
    return val;
}
/**
  * 函    数：遥控器数据解析
  * 参    数：无
  * 返 回 值：无
  */
uint8_t data_flag = 0;
void ps2_parse_data(uint8_t *str)
{
    unsigned int x1, x2, x3, x4, x5, x6, x7,x8;

    int res = sscanf((char*)str, "HUB0_Joystick data: x%02X x%02X x%02X x%02X x%02X x%02X x%02X x%02X",
                    &x1,&x2, &x3, &x4, &x5,&x6,&x7,&x8);

    // 只有解析成功8个数据，才更新结构体
    if(res == 8)
    {
        // 1. 填充原始硬件数据到结构体
        ps2.frame_id = x1;
				//映射+死区处理
			  ps2.lx = joystick_deadzone(map_joystick(x4));
        ps2.ly = joystick_deadzone(map_joystick(x5));
        ps2.buttons = (uint8_t)x7;

        ps2.left_key = 0; 
//			  ps2.right_key = 0; 
        if(ps2.frame_id == 0x01)
        {
            switch(x5)
            {
                case KEY_UP:
                    ps2.left_key = 1;
                    break;
                case KEY_DOWN:
                    ps2.left_key = 2;
                    break;
                default:
                    // 中位，所有按键松开
                    break;
            }
            switch(x4)
            {
                case KEY_LEFT:
                    ps2.left_key = 3;
                    break;
                case KEY_RIGHT:
                    ps2.left_key = 4;
                    break;
                default:
                    
                    break;
            }
						
						switch(x6)
            {
                case KEY_TRIANGLE:
                    ps2.right_key = 1;
                    break;
                case KEY_CROSS:
                    ps2.right_key = 2;
                    break;
								case KEY_SQUARE:
                    ps2.right_key = 3; 
                    break;
                case KEY_CIRCLE:
                    ps2.right_key = 4;
                    break;
                default:
                    ps2.right_key = 0;
                    break;
            }
            ps2.select = ((ps2.buttons & PS2_BUTTON_SELECT) != 0U) ? 1U : 0U;
            ps2.top_key = 0U;
            if ((ps2.buttons & PS2_BUTTON_L1) != 0U)
            {
                ps2.top_key = 1U;
            }
            else if ((ps2.buttons & PS2_BUTTON_L2) != 0U)
            {
                ps2.top_key = 2U;
            }
            else if ((ps2.buttons & PS2_BUTTON_R1) != 0U)
            {
                ps2.top_key = 3U;
            }
            else if ((ps2.buttons & PS2_BUTTON_R2) != 0U)
            {
                ps2.top_key = 4U;
            }
        }
				if (ps2.left_key != 0 || ps2.right_key != 0 || ps2.top_key != 0 || ps2.select != 0 || ps2.ly!= 0 || ps2.lx != 0) {
           data_flag = 1;
        } 
			 // ========== 新增：全零检测与超时标志 ==========
        int is_all_zero = (ps2.lx == 0 && ps2.ly == 0 &&
                           ps2.left_key == 0 && ps2.right_key == 0 &&
                           ps2.buttons == 0U);

        uint32_t now = HAL_GetTick();   // 获取当前系统毫秒数

        if (!is_all_zero) {
            // 有非零操作：清除停止标志，并刷新最后有效时间
            remote_stopped_flag = 0;
            last_nonzero_tick = now;
        } else {
            // 当前数据全为零：检查距离上次非零数据是否超过100ms
            if ((now - last_nonzero_tick) >= 100) {
                remote_stopped_flag = 1;
            }
					}
				
#if DEBUG_TEST == 5
//	 printf("PARSE: 帧%02X | LX:%d LY:%d | 左键:%d | 右键:%d | 顶键:%d |选择：%d\r\n",
//           ps2.frame_id, ps2.lx, ps2.ly,
//           ps2.left_key, ps2.right_key, ps2.top_key,ps2.select);
#endif
    }
//打印测试

		
}


