#ifndef __PID_H
#define __PID_H

#include <stdint.h>

/** 增量速度环：|target|、|actual| 均小于该值(m/s)时清零 PWM，可改大略放宽停稳判据 */
#ifndef PID_INC_VEL_DEADBAND_MS
#define PID_INC_VEL_DEADBAND_MS  (0.04f)
#endif

// PID 结构体
typedef struct {
    float target;   // 目标值
    float actual;   // 当前值
    float err;      // 当前误差
    float last_err; // 上一次误差
    float prev_err; // 上上次误差（增量式专用）

    float Kp;
    float Ki;
    float Kd;

    float out;      // 输出
    float max_out;  // 最大输出
    float integral; // 积分累加
} pid_typedef;

// 函数声明
void pid_init(pid_typedef *pid, float kp, float ki, float kd, float max_out);
float pid_calc(pid_typedef *pid);
float pid_calc_inc(pid_typedef *pid);

#endif

