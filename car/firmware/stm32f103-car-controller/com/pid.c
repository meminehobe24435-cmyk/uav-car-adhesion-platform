#include "pid.h"
#include <math.h>

// 初始化 PID
void pid_init(pid_typedef *pid, float kp, float ki, float kd, float max_out)
{
    pid->target = 0;
    pid->actual = 0;
    pid->err = 0;
    pid->last_err = 0;
    pid->prev_err = 0;

    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;

    pid->out = 0;
    pid->max_out = max_out;
	  pid->integral = 0;
}

float pid_calc(pid_typedef *pid)
{
    pid->err = pid->target - pid->actual; 

    // P
    float p = pid->Kp * pid->err;

    float i = pid->Ki * pid->integral;

    // D
    float d = pid->Kd * (pid->err - pid->last_err);

    pid->out = p + i + d;

    if (pid->out > pid->max_out) {
        pid->out = pid->max_out;
    }
    else if (pid->out < -pid->max_out) {
        pid->out = -pid->max_out;
    }
    else {
        pid->integral += pid->err;  
    }

    pid->last_err = pid->err;

    return pid->out;
}

float pid_calc_inc(pid_typedef *pid)
{
    if (fabsf(pid->target) < PID_INC_VEL_DEADBAND_MS) {
        pid->out = 0.0f;
        pid->err = 0.0f;
        pid->last_err = 0.0f;
        pid->prev_err = 0.0f;
        pid->integral = 0.0f;
        return 0.0f;
    }

    pid->err = pid->target - pid->actual;

    float increment =
        pid->Kp * (pid->err - pid->last_err)
      + pid->Ki * pid->err
      + pid->Kd * (pid->err - 2.0f * pid->last_err + pid->prev_err);

    if (increment > 3.0f) {
        increment = 3.0f;
    }
    if (increment < -3.0f) {
        increment = -3.0f;
    }

    pid->out += increment;

    if (pid->out > pid->max_out) {
        pid->out = pid->max_out;
    }
    if (pid->out < -pid->max_out) {
        pid->out = -pid->max_out;
    }

    pid->prev_err = pid->last_err;
    pid->last_err = pid->err;

    return pid->out;
}




