#ifndef IMU_H
#define IMU_H

#include <stdint.h>

/* 传感器数据结构体 */
typedef struct {
    float accel[3];      // 加速度，单位：g
    float gyro[3];       // 角速度，单位：°/s
    float mag[3];        // 磁场强度，原始ADC值（无缩放）
    float angle[3];      // 欧拉角，单位：度
} imu_data_t;

typedef struct {
    short accel[3];
    short gyro[3];
} imu_raw_data_t;

/* 模块初始化（创建互斥量等） */
void imu_init(void);

/* 读取传感器并更新内部数据（应周期性调用，例如每100ms） */
void imu_update(void);

/* 获取最新的传感器数据（线程安全，拷贝到用户结构体） */
void imu_get_data(imu_data_t *pData);

#endif /* IMU_H */

