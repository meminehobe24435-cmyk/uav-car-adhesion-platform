# 代码地图

## 1. 当前运行链路

```text
PS2 → CH559 → UART5 → ps2_usart5.c → main.c
→ car.c → motor.c + steering.c → servo.c
```

当前 `main.c` 直接把已解析 PS2 按键转换为车辆级 API 调用；`flight_comm` 和 `rc_control` 源码保留，但未进入当前运行路径。

## 2. STM32 工程

根路径：`car/firmware/stm32f103-car-controller`

| 文件 | 主要职责 |
|---|---|
| `Src/main.c` | 外设启动、PS2 500 ms 失联、按键锁存、30/60/80% 挡位、普通四轮转向 |
| `bsp/ps2_usart5.c` | UART5 单字节中断接收和 `HUB0_Joystick data` 解析 |
| `bsp/ps2_usart.h` | CH559 报告结构、面键和 Byte6 位定义 |
| `bsp/car.c` / `car.h` | 停车、直行、后退、普通转向、保留蟹行车辆 API |
| `bsp/motor.c` / `motor.h` | TIM2 四路 PWM、TB6612 方向/STBY、方向修正和限幅 |
| `bsp/steering.c` / `steering.h` | 回中、前后轴反向转向、四轮同向转向 |
| `bsp/servo.c` / `servo.h` | 四舵机标定、硬件/软件 PWM、安全限幅 |
| `app/flight_comm.c` / `flight_comm.h` | USART2 DMA、11 字节车辆协议、CRC8、500 ms 超时；当前未启动 |
| `app/rc_control.c` / `rc_control.h` | 通用输入抽象和失联保护；当前 PS2 主循环未接入 |
| `Src/tim.c` | TIM2 电机 PWM、TIM5 舵机、TIM1/3/4/8 编码器资源配置 |
| `Src/usart.c` | USART2、UART4、UART5 的引脚、波特率和 DMA/中断配置 |
| `Src/stm32f1xx_it.c` | 定时器、DMA、UART 中断入口 |
| `MDK-ARM/1_template_led.uvprojx` | Keil 工程入口 |
| `1_template_led.ioc` | CubeMX 配置快照；不要无审查地整工程重新生成 |

## 3. 关键 API

### 舵机

`servo_init`、`servo_set_us`、`servo_set_angle`、`servo_center`、`servo_center_all`、`servo_set_center`、`servo_set_direction`。

### 转向

`steering_center`、`steering_turn`、`steering_crab`。

### 电机

`motor_init`、`motor_set_pwm`、`motor_set_all`、`motor_stop_all`、`motor_percent_to_pwm`。

### 车辆

`car_init`、`car_stop`、`car_forward`、`car_backward`、`car_turn`、`car_crab`。

## 4. 飞控覆盖层

根路径：`adhesion/px4-overlay`

| 文件 | 主要职责 |
|---|---|
| `src/modules/height_commander/HeightCommander.cpp` | RC 边沿触发的相对高度状态机和 Offboard 位置目标 |
| `height_commander_params.yaml` | HC_ENABLE / HC_DELTA / HC_CHANNEL |
| `src/modules/mylink_bridge/MylinkBridge.cpp` | 危险的实验 ASCII 串口直接执行器桥，禁止实飞启用 |
| `mylink_bridge_params.yaml` | MLB_ENABLE / MLB_PORT / MLB_BAUDRATE |
| `boards/micoair/h743-v2/default.px4board` | 为 MicoAir H743-v2 编入 height_commander |
| `boards/px4/fmu-v6x/default.px4board` | 为 FMU-v6x 编入 mylink_bridge |
| `boards/px4/sitl/default.px4board` | SITL 编入两个实验模块 |

## 5. 生成物

当前发布 HEX 和对应构建日志位于：

`car/releases/PS2_NORMAL_TURN90_SPEED30_60_80/`

其他 Keil Objects/Listings、PX4 build、压缩包和第三方完整仓库不进入 Git。
