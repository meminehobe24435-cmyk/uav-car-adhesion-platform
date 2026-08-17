# STM32F103 四轮转向小车固件

## 工程入口

- MCU：STM32F103RCT6
- 控制板：WLKJ2025011 CAR-MOTOR-V1.2S
- Keil 工程：`firmware/stm32f103-car-controller/MDK-ARM/1_template_led.uvprojx`
- CubeMX 快照：`firmware/stm32f103-car-controller/1_template_led.ioc`
- 当前 HEX：`releases/PS2_NORMAL_TURN90_SPEED30_60_80/1_template_led.hex`
- 当前构建日志：`releases/PS2_NORMAL_TURN90_SPEED30_60_80/keil_rebuild.log`

HEX SHA256：

```text
ABE8271A1E9C588CF2D62D08F95AE26E29682AC93F7E971D0104B44E42962D17
```

## 舵机接口

| 位置 | 接口 | GPIO | PWM |
|---|---|---|---|
| LF | J9 | PA0 | TIM5_CH1 硬件 PWM |
| RF | J11 | PA1 | TIM5_CH2 硬件 PWM |
| LR | J12 | PC5 | TIM5 更新/CH3 比较中断软件 PWM |
| RR | J14 | PB0 | TIM5 更新/CH4 比较中断软件 PWM |

TIM5：72 MHz、PSC=71、ARR=19999、1 μs/计数、20 ms/50 Hz。标定表位于 `bsp/servo.c`，当前每路为 center=1500 μs、min=1300 μs、max=1700 μs、direction=+1。

## 电机接口

| ID | 接口 | TB6612 | PWM | 方向 GPIO | direction |
|---|---|---|---|---|---|
| MOTOR_1 | J1 | 1-A | TIM2_CH1 / PA15 | PC13 / PC14 | +1 |
| MOTOR_2 | J2 | 1-B | TIM2_CH2 / PB3 | PC0 / PC1 | +1 |
| MOTOR_3 | J3 | 2-A | TIM2_CH4 / PB11 | PB13 / PB12 | +1 |
| MOTOR_4 | J4 | 2-B | TIM2_CH3 / PB10 | PC9 / PC8 | -1 |

STBY1=PC15，STBY2=PB14。TIM2 ARR=99，满量程 100。当前上限 80%，PS2 挡位 30/60/80%。J1~J4 的完整 LF/RF/LR/RR 安装位置仍需贴标签确认，不应猜测。

## PS2 控制

PS2 无线接收器插入板载 USB-A，由 CH559 转为 UART5 数据。UART5 为 PC12/PD2、57600、8N1。

| 按键 | 动作 |
|---|---|
| L1 | 锁存直行 |
| A/Y | 锁存后退 |
| X/B | 锁存普通四轮左/右转并前进，命令角 ±90° |
| R1 | 停止并清除锁存 |
| L2 | 30%/60%/80% 循环切挡 |
| START | 停车并回中 |

500 ms 无有效 `0x01` 报告会停车、回中并清除锁存。当前不启用 SELECT、蟹行、编码器 PID、FreeRTOS 或飞控 UART。

## 编译和第一次测试

1. Keil Rebuild，确认 0 Error / 0 Warning；
2. 将四轮完全架空；
3. 上电确认电机停止、舵机回中；
4. 从 30% 挡开始测试 L1、A/Y、X、B、R1、START；
5. 最后测试关闭手柄/拔接收器后 500 ms 失联停车；
6. 只有低挡方向全部正确后，才测试 60% 和 80%。

更完整的设计与风险说明见 [技术开发文档](../docs/TECHNICAL_DEVELOPMENT_DOCUMENT.md)。
