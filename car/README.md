# 四舵轮小车固件

## 硬件与工程

- MCU：STM32F103RCT6
- 控制板：WLKJ2025011 CAR-MOTOR-V1.2S
- 工具链：Keil MDK-ARM / ARMCC 5.06 update 5
- 工程文件：`firmware/stm32f103-car-controller/MDK-ARM/1_template_led.uvprojx`
- CubeMX 配置：`firmware/stm32f103-car-controller/1_template_led.ioc`

## 舵机映射

| 位置 | 接口 | GPIO | 实现 |
|---|---|---|---|
| LF 左前 | J9 | PA0 | TIM5_CH1 硬件 PWM |
| RF 右前 | J11 | PA1 | TIM5_CH2 硬件 PWM |
| LR 左后 | J12 | PC5 | TIM5 更新/CH3 比较中断软件 PWM |
| RR 右后 | J14 | PB0 | TIM5 更新/CH4 比较中断软件 PWM |

TIM5 使用 72 MHz 时钟、PSC=71、ARR=19999，形成 1 us 计数和 20 ms 周期。四路舵机参数集中在 `bsp/servo.c` 的 `servo_config`。

## 电机映射

| 逻辑编号 | 接口 | TB6612 | PWM | 方向 GPIO |
|---|---|---|---|---|
| MOTOR_1 | J1 | TB6612_1 A | TIM2_CH1 / PA15 | PC13 / PC14 |
| MOTOR_2 | J2 | TB6612_1 B | TIM2_CH2 / PB3 | PC0 / PC1 |
| MOTOR_3 | J3 | TB6612_2 A | TIM2_CH4 / PB11 | PB13 / PB12 |
| MOTOR_4 | J4 | TB6612_2 B | TIM2_CH3 / PB10 | PC9 / PC8 |

两颗驱动的 STBY 分别为 PC15、PB14。TIM2 周期计数为 100，当前软件安全上限为 30%，首次测试值为 20%。J1~J4 到车辆 LF/RF/LR/RR 的安装对应以及四路 `direction` 仍需逐路实测后填写。

## 当前默认行为

`Src/main.c` 依次初始化 GPIO、TIM2、TIM5，执行：

1. `servo_init()`；
2. `steering_center()`；
3. `motor_init()`；
4. `motor_single_test()`。

因此固件会让四舵机保持中位，只对 J1 做停止、20% 正转、停止、20% 反转、停止的循环。FreeRTOS、编码器、PID、遥控、ROS 和飞控通信没有在当前 `main()` 中启动。

## 编译

1. 用 Keil 打开 `MDK-ARM/1_template_led.uvprojx`。
2. 选择 `1_template_led` target。
3. 执行 Rebuild。
4. 核对输出必须为 0 Error；本仓库保存的整理后全量重编译证据见 `docs/evidence/keil_rebuild_20260811.log`。

注意：构建成功只证明代码通过工具链，不等于四电机方向和机械安装已经验证。
