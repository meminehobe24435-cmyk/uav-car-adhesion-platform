#ifndef __ENCODER_H
#define __ENCODER_H

#include "tim.h"

/** 编码器换算后的轮速 (m/s) 反馈限幅，与 PID 使用同一量纲 */
#define ENCODER_ACTUAL_SPEED_LIMIT_MPS  (1.28f)

extern int read_encoder_count(TIM_HandleTypeDef* htim);

extern void encoder_start(void);


#endif
