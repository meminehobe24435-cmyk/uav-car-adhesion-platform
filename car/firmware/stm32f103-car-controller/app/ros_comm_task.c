/* MCU 涓?ROS 閫氳浠诲姟妯″潡 */
#include "ros_comm_task.h"
#include "usart.h"
#include "bsp_imu.h"
#include "motor.h"
#include <stdio.h>
extern volatile uint8_t ps2_active;

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os.h"

#define IP_CONFIRM_COUNT          50
#define ROS_COMM_RX_POLL_MS       20U
#define ROS_CONTROL_TIMEOUT_MS   300U

extern imu_raw_data_t g_imu_raw_data;   // IMU 鍘熷鏁版嵁缁撴瀯
extern Chassis_TypeDef chassis;         // 搴曠洏杩愬姩缁撴瀯浣?

trans_data_t send_data;
recv_data_t recv_data;

volatile static uint8_t recv_size = 0;

static SemaphoreHandle_t s_ros_uart_idle_sem = NULL;
static uint32_t s_last_control_tick = 0U;
static uint8_t s_control_received = 0U;

// IP 淇℃伅
uint8_t last_ip[4] = {0};
uint8_t recv_ip[4] = {0};
uint16_t ip_valid_cnt = 0;
uint8_t ip_frame_valid = 0;

// 涓插彛鐩稿叧鍑芥暟
static void _uart_trans_func(uint8_t* pdata, uint16_t len){
    HAL_UART_Transmit(&huart4, pdata, len, 100);
}

static void _uart_recv_func(uint8_t* pdata, uint16_t len){
    if (__HAL_UART_GET_FLAG(&huart4, UART_IT_IDLE) != RESET) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart4); // 娓呴櫎绌洪棽鏍囧織
    }
    __HAL_UART_ENABLE_IT(&huart4, UART_IT_IDLE); // 浣胯兘绌洪棽涓柇
    HAL_UART_Receive_DMA(&huart4, pdata, len);
}

void ros_comm_recv_idle_cb(void) {
	if(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_IDLE)){
		__HAL_UART_CLEAR_IDLEFLAG(&huart4);
		HAL_UART_DMAStop(&huart4);

		recv_size = RECEIVE_SIZE-__HAL_DMA_GET_COUNTER(huart4.hdmarx);
		if (s_ros_uart_idle_sem != NULL) {
			BaseType_t hp = pdFALSE;
			(void)xSemaphoreGiveFromISR(s_ros_uart_idle_sem, &hp);
			portYIELD_FROM_ISR(hp);
		}
//		printf("len is %d\r\n", recv_size);
	}
}

static uint8_t check_sum(unsigned char count_num,unsigned char mode)
{
	unsigned char check_sum=0, k;
    // 闇€瑕佸彂閫佺殑鏁版嵁杩涜鏍￠獙
	if(mode == 1)
	for(k=0; k<count_num; k++){
	    check_sum=check_sum^send_data.buffer[k];
	}
	
    // 瀵规帴鏀剁殑鏁版嵁杩涜鏍￠獙
	if(mode == 0)
	for(k=0; k<count_num; k++){
	    check_sum=check_sum^recv_data.buffer[k];
	}
	return check_sum;
}

static float xyz_speed_transition(uint8_t high_byte, uint8_t low_byte){
    short transition; 
    transition = ((high_byte << 8) + low_byte);
    return transition/1000+(transition%1000)*0.001;
}

static void data_transition(void){
    uint8_t stop_flag_temp = 0;     // 閫熷害鍋滄鏍囧織锛屽奖鍝嶅悗缁簲绛斿弬鏁?

    send_data.sensor_str.frame_header = FRAME_HEADER;
    send_data.sensor_str.frame_tail = FRAME_TAIL;

    // 鍙戦€佸簳鐩橀€熷害锛屾敞鎰忚涓?ROS 閬ユ帶绔殑姣斾緥淇濇寔涓€鑷?
    send_data.sensor_str.x_speed = (short)(chassis.cur_vx*1000/0.8/1.2);
    send_data.sensor_str.y_speed = (short)(chassis.cur_vy*1000/0.8/1.2);
    send_data.sensor_str.z_speed = (short)(chassis.cur_vz*1000/0.8/1.2);

    // 鍔犻€熷害
    send_data.sensor_str.imu_acc_x = g_imu_raw_data.accel[1]*8;    // IMU +y 瀵瑰簲 ROS 鍧愭爣绯?+x 鍓嶆柟
    send_data.sensor_str.imu_acc_y = -g_imu_raw_data.accel[0]*8;   // IMU -x 瀵瑰簲 ROS 鍧愭爣绯?+y 宸︽柟
    send_data.sensor_str.imu_acc_z = g_imu_raw_data.accel[2]*8;

    // 瑙掗€熷害
    send_data.sensor_str.imu_gyro_x = g_imu_raw_data.gyro[1]*4;
    send_data.sensor_str.imu_gyro_y = -g_imu_raw_data.gyro[0]*4;
//    if(chassis.mode == CHASSIS_MODE_STOP){      // 搴曠洏鍋滄鏃跺 z 杞磋閫熷害鍋忓樊鍋氬鐞?
//        stop_flag_temp = 1;
//        send_data.sensor_str.imu_gyro_z = 0;
//    }else{
//        stop_flag_temp = 0;
        send_data.sensor_str.imu_gyro_z = g_imu_raw_data.gyro[2]*4;
//    }

    // 鐢垫睜鐢靛帇锛屾殏鏃朵笉鍙備笌鍙戦€?
    send_data.sensor_str.power_voltage = 0;

    // 写锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
    send_data.buffer[0] = send_data.sensor_str.frame_header;
    send_data.buffer[1] = stop_flag_temp;

    send_data.buffer[2] = send_data.sensor_str.x_speed >> 8;
    send_data.buffer[3] = send_data.sensor_str.x_speed;
    send_data.buffer[4] = send_data.sensor_str.y_speed >> 8;
    send_data.buffer[5] = send_data.sensor_str.y_speed;
    send_data.buffer[6] = send_data.sensor_str.z_speed >> 8;
    send_data.buffer[7] = send_data.sensor_str.z_speed;

    send_data.buffer[8] = send_data.sensor_str.imu_acc_x >> 8;
    send_data.buffer[9] = send_data.sensor_str.imu_acc_x;
    send_data.buffer[10] = send_data.sensor_str.imu_acc_y >> 8;
    send_data.buffer[11] = send_data.sensor_str.imu_acc_y;
    send_data.buffer[12] = send_data.sensor_str.imu_acc_z >> 8;
    send_data.buffer[13] = send_data.sensor_str.imu_acc_z;

    send_data.buffer[14] = send_data.sensor_str.imu_gyro_x >> 8;
    send_data.buffer[15] = send_data.sensor_str.imu_gyro_x;
    send_data.buffer[16] = send_data.sensor_str.imu_gyro_y >> 8;
    send_data.buffer[17] = send_data.sensor_str.imu_gyro_y;
    send_data.buffer[18] = send_data.sensor_str.imu_gyro_z >> 8;
	send_data.buffer[19] = send_data.sensor_str.imu_gyro_z;

    send_data.buffer[20] = send_data.sensor_str.power_voltage >> 8;
    send_data.buffer[21] = send_data.sensor_str.power_voltage;

    send_data.buffer[22] = check_sum(22, 1);
    send_data.buffer[23] = send_data.sensor_str.frame_tail;
}

/**
 * @brief ROS 涓嬩綅鏈烘帶鍒舵寚浠ゅ鐞?
 * @note 鏀寔鏍煎紡锛氬抚澶?+ 鏁版嵁 + IP甯?+ 甯у熬
 */
// ROS 鎸囦护绫诲瀷鏋氫妇
enum{
    ROS_CMD_TYPE_CONTROL = 0,   // 鎺у埗鎸囦护
    ROS_CMD_TYPE_RECHARGE1,     // 鍏呯數鎸囦护1
    ROS_CMD_TYPE_RECHARGE2,     // 鍏呯數鎸囦护2
    ROS_CMD_TYPE_INFRFRED,      // 绾㈠鎸囦护
    ROS_CMD_TYPE_IP = 0xFF,     // IP 鎸囦护
};


static void ros_comm_recv_proc(void){
    if(recv_data.buffer[0] != FRAME_HEADER || recv_data.buffer[10] != FRAME_TAIL || 
        recv_data.buffer[9] != check_sum(9, 0)){
        return ;
    }
    uint8_t cmd_type = recv_data.buffer[1];
	  uint8_t temp_ip[4];
	
    switch(cmd_type){
        case ROS_CMD_TYPE_CONTROL:
            if (ps2_active) break;
            // 璁剧疆搴曠洏閫熷害锛屾敞鎰忚涓?ROS 閬ユ帶绔殑鏄犲皠淇濇寔涓€鑷?
            chassis.vx = -xyz_speed_transition(recv_data.buffer[3], recv_data.buffer[4]);
            chassis.vy = xyz_speed_transition(recv_data.buffer[5], recv_data.buffer[6]);
            chassis.vz = -xyz_speed_transition(recv_data.buffer[7], recv_data.buffer[8]);
            s_last_control_tick = HAL_GetTick();
            s_control_received = 1U;
            break;
        case ROS_CMD_TYPE_IP:
            // 璇诲彇鎺ユ敹鍒扮殑 IP锛屼綅浜?3-6 瀛楄妭
            temp_ip[0] = recv_data.buffer[3];
            temp_ip[1] = recv_data.buffer[4];
            temp_ip[2] = recv_data.buffer[5];
            temp_ip[3] = recv_data.buffer[6];

            // 妫€鏌ユ湰娆?IP 鏄惁涓庝笂涓€娆＄浉鍚?
            if(temp_ip[0] == last_ip[0] && temp_ip[1] == last_ip[1] && 
                temp_ip[2] == last_ip[2] && temp_ip[3] == last_ip[3]){
                ip_valid_cnt++;
                if(ip_valid_cnt >= IP_CONFIRM_COUNT){
                    recv_ip[0] = temp_ip[0];
                    recv_ip[1] = temp_ip[1];
                    recv_ip[2] = temp_ip[2];
                    recv_ip[3] = temp_ip[3];
                    ip_frame_valid = 1;
                }
            }
            else{
                ip_valid_cnt = 1;   // 閲嶆柊璁℃暟
                last_ip[0] = temp_ip[0];
                last_ip[1] = temp_ip[1];
                last_ip[2] = temp_ip[2];
                last_ip[3] = temp_ip[3];
            }
            break;
        default:
            // 鍏朵粬鎸囦护鏆備笉澶勭悊
            break;
    }
}

/**
 * @brief ROS 閫氳鍒濆鍖?
 */
static void ros_comm_init(void){
    s_ros_uart_idle_sem = xSemaphoreCreateBinary();
    // 璁剧疆涓插彛 + DMA + IDLE 鎺ユ敹娴佺▼
    __HAL_UART_ENABLE_IT(&huart4, UART_IT_IDLE); // 鎵嬪姩浣胯兘绌洪棽涓柇
    _uart_recv_func(recv_data.buffer, RECEIVE_SIZE);
}

/**
 * @note 20Hz 鍛ㄦ湡浠诲姟
 */
void ros_comm_task(void const *argument){
    ros_comm_init();
    while(1){
		if (s_ros_uart_idle_sem != NULL &&
		    xSemaphoreTake(s_ros_uart_idle_sem, pdMS_TO_TICKS(ROS_COMM_RX_POLL_MS)) == pdTRUE) {
			if(recv_size == RECEIVE_SIZE){
				ros_comm_recv_proc();
			}
			_uart_recv_func(recv_data.buffer, RECEIVE_SIZE);
		}

		// Keep a 10 Hz command stream continuous, but stop on a real link loss.
		if (!ps2_active && s_control_received &&
		    (uint32_t)(HAL_GetTick() - s_last_control_tick) > ROS_CONTROL_TIMEOUT_MS) {
			chassis.vx = 0.0f;
			chassis.vy = 0.0f;
			chassis.vz = 0.0f;
			s_control_received = 0U;
		}
		
// 		// 娴嬭瘯鍙戦€佸抚
//		uint8_t temp_buf[24] = {0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x96, 0xFF, 0x38, 0x40, 0x00, 0xFF, 0xF6, 0x00, 0x05, 0xFF, 0xFE, 0x2E, 0xE0, 0xA9, 0x7D};
//        _uart_trans_func(temp_buf, TRANSMIT_SIZE);
//		osDelay(50);
		
		data_transition();			
		_uart_trans_func(send_data.buffer, TRANSMIT_SIZE);
		osDelay(20);
    }
}
