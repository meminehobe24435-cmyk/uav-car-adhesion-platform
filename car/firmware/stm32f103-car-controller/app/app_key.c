#include "app_key.h"
#include "mid_key.h"
#include "com_debug.h"
#include "chassis_task.h"
void up_btn_evt_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;
    switch (btn->event) {

        case FLEX_BTN_PRESS_CLICK:                //单机
          break;
				
        case FLEX_BTN_PRESS_DOUBLE_CLICK:         //双击
			    break;
				
        case FLEX_BTN_PRESS_LONG_HOLD_UP:         //长按保持后抬起事件 
          break;
				
				case FLEX_BTN_PRESS_LONG_HOLD:            //长按保持时间
					break;
				
        default:
					break;
           
    }
}
//拓展按键回调
void down_btn_evt_cb(void *arg)
{
	   
}

void mid_btn_evt_cb(void *arg)
{
  
}

void right_btn_evt_cb(void *arg)
{
	
}

void left_btn_evt_cb(void *arg)
{
	
}

