# 代码地图

## 小车 STM32 工程

| 路径 | 作用 |
|---|---|
| `car/firmware/stm32f103-car-controller/Src/main.c` | 当前最小启动流程和 MOTOR_1 测试入口 |
| `bsp/servo.c`, `bsp/servo.h` | 四舵机配置、限幅、硬件/软件 PWM、零位标定 API |
| `bsp/steering.c`, `bsp/steering.h` | 回中、蟹行、前后轴反向基础转向 |
| `bsp/motor.c`, `bsp/motor.h` | 四路 TB6612 PWM、方向、STBY、安全限幅和测试 API |
| `Src/tim.c`, `Inc/tim.h` | TIM2 电机 PWM 与 TIM5 舵机时基 |
| `Src/gpio.c`, `Inc/main.h` | 板级 GPIO 初始化与管脚定义 |
| `MDK-ARM/1_template_led.uvprojx` | Keil 工程入口 |
| `1_template_led.ioc` | CubeMX 配置快照；本仓库没有重新生成工程 |

### 主要 API

舵机：`servo_init`、`servo_set_us`、`servo_center_all`、`servo_set_angle`。

转向：`steering_center`、`steering_crab`、`steering_turn`。

电机：`motor_init`、`motor_set_pwm`、`motor_stop_all`、`motor_set_all`、`car_forward`、`car_backward`。

## PX4 覆盖层

| 路径 | 作用 |
|---|---|
| `adhesion/px4-overlay/src/modules/height_commander/` | RC 开关触发相对高度变化状态机 |
| `adhesion/px4-overlay/src/modules/mylink_bridge/` | 串口文本命令到 PX4 命令/执行器的实验桥接 |
| `boards/micoair/h743-v2/default.px4board` | 为 MicoAir H743-v2 编入 `height_commander` |
| `boards/px4/sitl/default.px4board` | 为 SITL 编入两个实验模块 |
| `boards/px4/fmu-v6x/default.px4board` | 为 FMU-v6x 编入 `mylink_bridge` |

PX4 部分是覆盖层，不包含完整上游。使用时必须先准备 `SOURCE_MANIFEST.md` 指定的基线。
