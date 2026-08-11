#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "remote_task.h"
#include "motor.h"
#include "vofa.h"
#include "chassis_task.h"
#include "calc_fun.h"
#include "com_debug.h"
#include "bsp_imu.h"


void remote_task(void const * argument)
{
	  imu_init();
		
    while (1)
    {
			 imu_update();

       osDelay(1);         
    }

}
