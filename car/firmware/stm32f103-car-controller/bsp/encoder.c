#include "encoder.h"
#include "stm32f1xx_hal.h"
#include "tim.h"
#include "chassis_task.h"

void encoder_start(void)
{
  HAL_TIM_Encoder_Start(&htim3,TIM_CHANNEL_1);  
	HAL_TIM_Encoder_Start(&htim3,TIM_CHANNEL_2);  
	HAL_TIM_Encoder_Start(&htim8,TIM_CHANNEL_1); 
	HAL_TIM_Encoder_Start(&htim8,TIM_CHANNEL_2);  
	
  HAL_TIM_Encoder_Start(&htim1,TIM_CHANNEL_1); 
	HAL_TIM_Encoder_Start(&htim1,TIM_CHANNEL_2);
	HAL_TIM_Encoder_Start(&htim4,TIM_CHANNEL_1); 
	HAL_TIM_Encoder_Start(&htim4,TIM_CHANNEL_2);
	HAL_TIM_Base_Start_IT(&htim6);								
}

int read_encoder_count(TIM_HandleTypeDef* htim)
{
  int temp = (short)__HAL_TIM_GetCounter(htim); 
	__HAL_TIM_SetCounter(htim,0);
	return temp;
}

float Encoder_Calc_Mps(float pulse)
{
    return (pulse * PI * WHEEL_DIAMETER) / (ENC_LINE * ENC_MULTIPLE * GEAR_RATIO * CAPTURE_PERIOD);
}

float limit_float(float value, float min, float max)
{
    if(value > max) return max;
    if(value < min) return min;
    return value;
}

static float filter_encoder_speed(uint8_t index, float speed)
{
    static float filtered_speed[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float alpha = 0.2f;

    filtered_speed[index] += alpha * (speed - filtered_speed[index]);
    return filtered_speed[index];
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == TIM6)
  {
		 chassis.motor1_pid[0].actual =  -read_encoder_count(&htim8);
		 chassis.motor1_pid[0].actual =  Encoder_Calc_Mps(chassis.motor1_pid[0].actual);
		 chassis.motor1_pid[0].actual = limit_float(chassis.motor1_pid[0].actual, -ENCODER_ACTUAL_SPEED_LIMIT_MPS, ENCODER_ACTUAL_SPEED_LIMIT_MPS); // ???
		 chassis.motor1_pid[0].actual = filter_encoder_speed(0, chassis.motor1_pid[0].actual);
     float pwm0 = pid_calc_inc(&chassis.motor1_pid[0]);
	   chassis.motor[0].speed = (int16_t)pwm0;

		 chassis.motor1_pid[1].actual =  read_encoder_count(&htim3);
		 chassis.motor1_pid[1].actual =  Encoder_Calc_Mps(chassis.motor1_pid[1].actual);
		 chassis.motor1_pid[1].actual = limit_float(chassis.motor1_pid[1].actual, -ENCODER_ACTUAL_SPEED_LIMIT_MPS, ENCODER_ACTUAL_SPEED_LIMIT_MPS); // ???
		 chassis.motor1_pid[1].actual = filter_encoder_speed(1, chassis.motor1_pid[1].actual);
	   float pwm1 = pid_calc_inc(&chassis.motor1_pid[1]);
	   chassis.motor[1].speed = (int16_t)pwm1;
		
		 chassis.motor1_pid[2].actual =  read_encoder_count(&htim1);
		 chassis.motor1_pid[2].actual =  Encoder_Calc_Mps(chassis.motor1_pid[2].actual);
		 chassis.motor1_pid[2].actual = limit_float( chassis.motor1_pid[2].actual, -ENCODER_ACTUAL_SPEED_LIMIT_MPS, ENCODER_ACTUAL_SPEED_LIMIT_MPS); // ???
		 chassis.motor1_pid[2].actual = filter_encoder_speed(2, chassis.motor1_pid[2].actual);
     float pwm2 = pid_calc_inc(&chassis.motor1_pid[2]);
	   chassis.motor[2].speed = (int16_t)pwm2;

		 chassis.motor1_pid[3].actual =  -read_encoder_count(&htim4);
		 chassis.motor1_pid[3].actual =  Encoder_Calc_Mps(chassis.motor1_pid[3].actual);
		 chassis.motor1_pid[3].actual = limit_float(chassis.motor1_pid[3].actual, -ENCODER_ACTUAL_SPEED_LIMIT_MPS, ENCODER_ACTUAL_SPEED_LIMIT_MPS); // ???
		 chassis.motor1_pid[3].actual = filter_encoder_speed(3, chassis.motor1_pid[3].actual);
     float pwm3 = pid_calc_inc(&chassis.motor1_pid[3]);
	   chassis.motor[3].speed = (int16_t)pwm3;
		 
		 float s0 = chassis.motor1_pid[0].actual;
		 float s1 = chassis.motor1_pid[1].actual;
		 float s2 = chassis.motor1_pid[2].actual;
		 float s3 = chassis.motor1_pid[3].actual;
		
		 // Encoder signs are normalized above; positive average is ROS forward.
		 float left_avg = (s0 + s3) * 0.5f;
		 float right_avg = (s1 + s2) * 0.5f;
		 chassis.cur_vx = (left_avg + right_avg) * 0.5f;   // ROS +x is forward
		 chassis.cur_vy = 0.0f;
		 chassis.cur_vz = (right_avg - left_avg) / (2.0f * WHEEL_BASE_SUM);  // ROS +z is left turn
		 motor_ctrol(&chassis);
  }
}
