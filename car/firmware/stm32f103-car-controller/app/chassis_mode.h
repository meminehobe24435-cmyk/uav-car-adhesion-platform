#ifndef __CHASSIS_MODE_H
#define __CHASSIS_MODE_H


#include "stdint.h"

#include "pid.h"
typedef struct
{
    int16_t speed;         // 最终输出值
	  int16_t target_speed;  //目标速度

} Motor_TypeDef;
// 1. 定义底盘四种工作模式
typedef enum {
    CHASSIS_MODE_STOP = 0,        // 停止
    CHASSIS_MODE_ONLY_FORWARD,    // 纯前进后退
    CHASSIS_MODE_FORWARD_TURN,    // 前进/后退 + 转向差速转向
    CHASSIS_MODE_ROTATE,          // 原地旋转    
}chassis_mode_typedef;

// 2. 底盘控制结构体（包含模式 + 所有控制量）
typedef struct {
   chassis_mode_typedef mode;                      // 当前底盘模式
   pid_typedef motor1_pid[4];                      //四个电机pid结构体
   Motor_TypeDef motor[4];                         //电机参数结构体
	 float vx;                               // x方向速度 (m/s)
	 float vy;                               // y方向速度 (m/s)
	 float vz;                               // z方向速度 
	
	float cur_vx;		// 当前x方向的实际速度(m/s)
	float cur_vy;		// 当前y方向的实际速度(m/s)
	float cur_vz;		// 当前z方向的实际速度(m/s)
} Chassis_TypeDef;

// 全局底盘结构体
extern Chassis_TypeDef chassis;

#endif

