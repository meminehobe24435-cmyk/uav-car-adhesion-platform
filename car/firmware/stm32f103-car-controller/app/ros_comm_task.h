#ifndef  __ROS_COMM_TASK_H
#define  __ROS_COMM_TASK_H

#include <stdint.h>

#define FRAME_HEADER      0X7B
#define FRAME_TAIL        0X7D
#define TRANSMIT_SIZE   24
#define RECEIVE_SIZE    11

typedef struct {
    uint8_t buffer[TRANSMIT_SIZE];
    struct _sensor_str {
        uint8_t frame_header;
        short x_speed;
        short y_speed;
        short z_speed;
        short power_voltage;
        short imu_acc_x;
        short imu_acc_y;
        short imu_acc_z;
        short imu_gyro_x;
        short imu_gyro_y;
        short imu_gyro_z;
        uint8_t frame_tail;
    }sensor_str;
}trans_data_t;

typedef struct {
    uint8_t buffer[RECEIVE_SIZE];
    struct _control_str {
        uint8_t frame_header;
        short x_speed;
        short y_speed;
        short z_speed;
        uint8_t frame_tail;
    }control_str;
}recv_data_t;

void ros_comm_recv_idle_cb(void);
void ros_comm_task(void const *argument);

#endif

