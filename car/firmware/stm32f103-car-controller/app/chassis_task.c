#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "string.h"
#include "tim.h"

#include "chassis_task.h"
#include "motor.h"
#include "ps2_usart.h"
#include "encoder.h"
#include "com_debug.h"
#include "vofa.h"
#include "pid.h"
#include "math.h"
#include "chassis_mode.h"
#include "mid_key.h"
#include "app_key.h"
#include "stdlib.h"
float a,b,c,d,e,f,g,h;             
volatile uint8_t ps2_active = 0;
static uint32_t ps2_last_active_tick = 0;

Chassis_TypeDef chassis;           

static void chassis_init(void);
static void motor_pid_init(Chassis_TypeDef* chassis);
static void data_parsing_speed(PS2_HandleTypeDef* ps2);
static void decompose_and_normalize(Chassis_TypeDef *chassis);

void chassic_task(void const * argument)
{
	user_button_init();
  chassis_init();
  while(1)
	{
    if(uart5_rx_finish == 1)
	  {
			ps2_parse_data(uart5_rx_buf);
			uart5_rx_finish = 0;
			memset(uart5_rx_buf, 0, RX_BUF_SIZE);
			if( remote_stopped_flag == 0)
			{
				if(data_flag == 1)
				{
				 data_parsing_speed(&ps2);  
				 decompose_and_normalize(&chassis);
			   data_flag =0 ;
			   ps2_active = 1;
			   ps2_last_active_tick = HAL_GetTick();
				}
			}else{
			   chassis.vx = 0;
         chassis.vy = 0;
         chassis.vz = 0;
         ps2_active = 0;
				 decompose_and_normalize(&chassis);
			}
		}
	  else{
		if (ps2_active && HAL_GetTick() - ps2_last_active_tick > 500) {
		    ps2_active = 0;
		}
		decompose_and_normalize(&chassis);
	  }

    osDelay(25); 
  }
}


static void chassis_init(void)
{
  uasrt_rx_init();              
	motor_init();									
  encoder_start();							 
	motor_pid_init(&chassis);							
	
	chassis.vx = 0;
	chassis.vy = 0;
	chassis.vz = 0;
}

static void motor_pid_init(Chassis_TypeDef* chassis)
{
    pid_init(&chassis->motor1_pid[0], 282.0f, 85.0f, 0.0f, 100);
    pid_init(&chassis->motor1_pid[1], 282.0f, 85.0f, 0.0f, 100);
    pid_init(&chassis->motor1_pid[2], 282.0f, 85.0f, 0.0f, 100);
    pid_init(&chassis->motor1_pid[3], 282.0f, 85.0f, 0.0f, 100);
}

static void data_parsing_speed(PS2_HandleTypeDef* ps2)
{
	 if(ps2->frame_id != 0x01) return;
   chassis.vx = ps2->ly / 100.0f;
	 
	 chassis.vz = ps2->lx / 100.0f * 5;
		
	 if(ps2->right_key == 3) chassis.vy = 0.8;
	 else if(ps2->right_key == 4) chassis.vy = -0.8;
	 else if(ps2->right_key == 0) chassis.vy = 0;
   
#if DEBUG_TEST == 1
	printf("  speed:vy:%.2f  vz:%.2f vx:%.2f \r\n",chassis.vy,chassis.vz,chassis.vx);
	printf("  speed:vy:%d  vz:%d  \r\n",ps2->lx,ps2->ly);
#endif
}


void motor_pid_test(void)
{
    static int step = 0;

    step++;
    if(step > 4) 
        step = 0;

    switch(step)
    {
        case 0: 
								chassis.motor1_pid[2].target = -1.0f; 
								chassis.motor1_pid[3].target = -1.0f;
								chassis.motor1_pid[0].target = -1.0f; 
								chassis.motor1_pid[1].target = -1.0f;
			  break;
        case 1: 
								chassis.motor1_pid[2].target = -0.5f; 
			          chassis.motor1_pid[3].target = -0.5f; 
								chassis.motor1_pid[0].target = -0.5f; 
			          chassis.motor1_pid[1].target = -0.5f; 
			  break;
        case 2: 
								chassis.motor1_pid[2].target =  0.0f; 
								chassis.motor1_pid[3].target =  0.0f; 
								chassis.motor1_pid[0].target =  0.0f; 
								chassis.motor1_pid[1].target =  0.0f; 
			  break;
        case 3: 
								chassis.motor1_pid[2].target =  0.5f; 
			          chassis.motor1_pid[3].target =  0.5f;
				        chassis.motor1_pid[0].target =  0.5f; 
			          chassis.motor1_pid[1].target =  0.5f;
			  break;
        case 4: 
					      chassis.motor1_pid[2].target =  1.0f; 
								chassis.motor1_pid[3].target =  1.0f; 
								chassis.motor1_pid[0].target =  1.0f; 
								chassis.motor1_pid[1].target =  1.0f; 
			  break;
        default: break;
    }
#if DEBUG_TEST == 0
//		vofa_printf()
#endif 
	
}

static void decompose_and_normalize(Chassis_TypeDef *chassis)
{
    float wz_term = WHEEL_BASE_SUM * chassis->vz;

    float motor_speed[4];
    motor_speed[3] = chassis->vx - chassis->vy - wz_term;  
    motor_speed[1] = chassis->vx + chassis->vy + wz_term;   
    motor_speed[0] = chassis->vx + chassis->vy - wz_term;   
    motor_speed[2] = chassis->vx - chassis->vy + wz_term;   


    float max_abs = fabs(motor_speed[0]);
    max_abs = fmax(max_abs, fabs(motor_speed[1]));
    max_abs = fmax(max_abs, fabs(motor_speed[2]));
    max_abs = fmax(max_abs, fabs(motor_speed[3]));

    if (max_abs > MAX_WHEEL_SPEED) {
        float scale = MAX_WHEEL_SPEED / max_abs;
        for (int i = 0; i < 4; i++) {
            motor_speed[i] *= scale;
        }
    }

    for (int i = 0; i < 4; i++) {
        const float max_target_step = 0.01f;

        if (fabsf(motor_speed[i]) < PID_INC_VEL_DEADBAND_MS) {
            chassis->motor1_pid[i].target = 0.0f;
        } else {
            float target_delta = motor_speed[i] - chassis->motor1_pid[i].target;
            if (target_delta > max_target_step) {
                target_delta = max_target_step;
            } else if (target_delta < -max_target_step) {
                target_delta = -max_target_step;
            }
            chassis->motor1_pid[i].target += target_delta;
        }
    }
#if DEBUG_TEST == 1
    static TickType_t last_print_time = 0;
    TickType_t now = xTaskGetTickCount();
    if ((now - last_print_time) >= pdMS_TO_TICKS(200)) {
        last_print_time = now;
         printf("Motor targets: FL=%.2f, FR=%.2f, RL=%.2f, RR=%.2f\r\n",
               chassis->motor1_pid[3].target,
               chassis->motor1_pid[1].target,
               chassis->motor1_pid[0].target,
               chassis->motor1_pid[2].target);
    }
#endif
	
}
