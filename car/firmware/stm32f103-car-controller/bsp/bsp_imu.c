#include "bsp_imu.h"
#include <stdio.h>
#include "bsp_iic.h"           
#include "com_debug.h"

// 定义全局变量（非静态）
imu_data_t g_imu_data;
imu_raw_data_t g_imu_raw_data;
//imu_raw_data_t g_imu_raw_err;

//uint32_t acc_x_err = 0, acc_y_err = 0, acc_z_err = 0;
//uint32_t gyro_x_err = 0, gyro_y_err = 0, gyro_z_err = 0;
	
static int16_t CharToShort(const unsigned char* buf) {
    return (int16_t)(buf[0] | (buf[1] << 8));
}

void imu_init(void) {
    for (int i = 0; i < 3; i++) {
        g_imu_data.accel[i] = 0.0f;
        g_imu_data.gyro[i]  = 0.0f;
        g_imu_data.mag[i]   = 0.0f;
        g_imu_data.angle[i] = 0.0f;
    }
	
//	unsigned char chrTemp[32];
//	int cnt = 0;
//	
//	for(int i=0; i<250; i++){
//		if (IIC_ReadBytes(0x50, 0x30, 32, chrTemp) != 32) {
//			continue;
//		}
//		cnt ++;
//		acc_x_err += CharToShort(&chrTemp[0 + 8]);
//		acc_y_err += CharToShort(&chrTemp[2 + 8]);
//		acc_z_err += CharToShort(&chrTemp[4 + 8]);

//		gyro_x_err += CharToShort(&chrTemp[6 + 8]);
//		gyro_y_err += CharToShort(&chrTemp[8 + 8]);
//		gyro_z_err += CharToShort(&chrTemp[10 + 8]);
//	}
//	
//	g_imu_raw_err
}

void imu_update(void) {
    unsigned char chrTemp[32];

    if (IIC_ReadBytes(0x50, 0x30, 32, chrTemp) != 32) {
        printf("IMU: IIC read error!\r\n");
        return;
    }

    // 赋值原始数据（以供串口上传到ROS）
    g_imu_raw_data.accel[0] = CharToShort(&chrTemp[0 + 8]);
    g_imu_raw_data.accel[1] = CharToShort(&chrTemp[2 + 8]);
    g_imu_raw_data.accel[2] = CharToShort(&chrTemp[4 + 8]);

    g_imu_raw_data.gyro[0] = CharToShort(&chrTemp[6 + 8]);
    g_imu_raw_data.gyro[1] = CharToShort(&chrTemp[8 + 8]);
    g_imu_raw_data.gyro[2] = CharToShort(&chrTemp[10 + 8]);

    // 直接填充全局变量
    g_imu_data.accel[0] = (float)g_imu_raw_data.accel[0] / 32768.0f * 16.0f;
    g_imu_data.accel[1] = (float)g_imu_raw_data.accel[1] / 32768.0f * 16.0f;
    g_imu_data.accel[2] = (float)g_imu_raw_data.accel[2] / 32768.0f * 16.0f;

    g_imu_data.gyro[0]  = (float)g_imu_raw_data.gyro[0] / 32768.0f * 2000.0f;
    g_imu_data.gyro[1]  = (float)g_imu_raw_data.gyro[1] / 32768.0f * 2000.0f;
    g_imu_data.gyro[2]  = (float)g_imu_raw_data.gyro[2] / 32768.0f * 2000.0f;

    g_imu_data.mag[0]   = (float)CharToShort(&chrTemp[12 + 8]);
    g_imu_data.mag[1]   = (float)CharToShort(&chrTemp[14 + 8]);
    g_imu_data.mag[2]   = (float)CharToShort(&chrTemp[16 + 8]);

    g_imu_data.angle[0] = (float)CharToShort(&chrTemp[18 + 8]) / 32768.0f * 180.0f;
    g_imu_data.angle[1] = (float)CharToShort(&chrTemp[20 + 8]) / 32768.0f * 180.0f;
    g_imu_data.angle[2] = (float)CharToShort(&chrTemp[22 + 8]) / 32768.0f * 180.0f;

#if DEBUG_TEST == 2
    printf("IMU: accel: %.3f, %.3f, %.3f; gyro: %.3f, %.3f, %.3f; mag: %.0f, %.0f, %.0f; angle: %.3f, %.3f, %.3f\r\n",
           g_imu_data.accel[0], g_imu_data.accel[1], g_imu_data.accel[2],
           g_imu_data.gyro[0],  g_imu_data.gyro[1],  g_imu_data.gyro[2],
           g_imu_data.mag[0],   g_imu_data.mag[1],   g_imu_data.mag[2],
           g_imu_data.angle[0], g_imu_data.angle[1], g_imu_data.angle[2]);
#endif
}
