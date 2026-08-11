# 四直流电机独立驱动与首次测试

## 1. 接口与TB6612通道

原理图能够确认J1~J4的电气通道，但不能确认四个接口在实车上的LF/RF/LR/RR安装位置。因此当前代码只使用MOTOR_1~MOTOR_4：

| MotorId | 底板接口 | 驱动通道 | 电机输出 | 编码器资源（当前不启用） |
|---|---|---|---|---|
| MOTOR_1 | J1 | TB6612_1 A | AO1/AO2 | TIM3_CH1/CH2 |
| MOTOR_2 | J2 | TB6612_1 B | BO1/BO2 | TIM4_CH1/CH2 |
| MOTOR_3 | J3 | TB6612_2 A | AO1/AO2 | TIM8_CH1/CH2 |
| MOTOR_4 | J4 | TB6612_2 B | BO1/BO2 | TIM1_CH1/CH2 |

每个6Pin接口的1、6脚是电机输出，2脚是5V，3、4脚是编码器AB相，5脚是GND。首次动力测试不初始化、不读取3/4脚编码器。

## 2. PWM通道与频率

| MotorId | TIM2通道 | PWM GPIO | TB6612 PWM输入 |
|---|---|---|---|
| MOTOR_1 / J1 | TIM2_CH1 | PA15 | TB6612_1 PWMA |
| MOTOR_2 / J2 | TIM2_CH2 | PB3 | TB6612_1 PWMB |
| MOTOR_3 / J3 | TIM2_CH4 | PB11 | TB6612_2 PWMA |
| MOTOR_4 / J4 | TIM2_CH3 | PB10 | TB6612_2 PWMB |

TIM2时钟为72 MHz，PSC=35，ARR=99：

```text
PWM频率 = 72 MHz / (35 + 1) / (99 + 1) = 20 kHz
```

一个周期有100个计数状态，因此软件使用100作为100%满量程命令。CCR=20为20%占空比；当前安全上限CCR=30，即30%。

## 3. 方向GPIO

| MotorId | 正方向IN1 | 反方向IN2 |
|---|---|---|
| MOTOR_1 / J1 | TB6612_1_AIN1 = PC13 | TB6612_1_AIN2 = PC14 |
| MOTOR_2 / J2 | TB6612_1_BIN1 = PC0 | TB6612_1_BIN2 = PC1 |
| MOTOR_3 / J3 | TB6612_2_AIN1 = PB13 | TB6612_2_AIN2 = PB12 |
| MOTOR_4 / J4 | TB6612_2_BIN1 = PC9 | TB6612_2_BIN2 = PC8 |

底层在改变方向GPIO前先把对应PWM清零，以降低带载换向冲击。停止模式为PWM=0且IN1=IN2=0（滑行停止）。

## 4. STBY控制

- TB6612_1_STBY = PC15。
- TB6612_2_STBY = PB14。
- `MX_GPIO_Init()`上电先把两路STBY初始化为低。
- `motor_init()`保持STBY低，清零全部方向和CCR，确认四路PWM启动成功后才把两路STBY拉高。

## 5. 安全PWM与direction

四路参数集中在`bsp/motor.c`顶部：

```c
MotorConfig motor_config[MOTOR_COUNT] =
{
  {1, 30U}, /* MOTOR_1 / J1 */
  {1, 30U}, /* MOTOR_2 / J2 */
  {1, 30U}, /* MOTOR_3 / J3 */
  {1, 30U}  /* MOTOR_4 / J4 */
};
```

- 第一个数是`direction`，只使用+1或-1。
- 第二个数是`pwm_max`，本阶段全部为30，即最大30%。
- `motor_set_pwm()`先应用direction，再强制限制到该路pwm_max。
- 当前四路direction都是+1，但尚未通过实车确认车辆前进方向。

## 6. 默认单电机测试

当前`main.c`只运行MOTOR_1，也就是J1：

1. 四个舵机进入各自center，保持直行。
2. 四个电机停止2秒。
3. MOTOR_1以CCR=20正转2秒。
4. 四个电机停止2秒。
5. MOTOR_1以CCR=20反转2秒。
6. 四个电机停止3秒。
7. 循环。

MOTOR_2、MOTOR_3、MOTOR_4始终停止。首次上电前应把四轮悬空，确认J1只连接一个待测电机，检查12.6V动力电源极性和接插件，再手动烧录。发现接线发热、驱动异常或机械卡滞应立即断电。

`motor_four_test()`也已实现，但没有设为默认入口。它按MOTOR_1~4逐个执行+20%、停止、-20%、停止，任何时刻只有一路转动。

## 7. 基础车辆接口

- `car_stop()`：四路PWM清零并将方向输入置低。
- `car_forward(pwm)`：向四路发送相同的车辆坐标正命令。
- `car_backward(pwm)`：向四路发送相同的车辆坐标负命令。

在J1~J4的实车轮位和实际正方向确认前，不要自动调用`car_forward()`或`car_backward()`。后续只通过每路MotorConfig.direction校正镜像安装，不在上层交换单轮正负号。

## 8. 后续加入编码器

完成四个接口的轮位和正方向确认后，再按J1=TIM3、J2=TIM4、J3=TIM8、J4=TIM1逐路启动编码器。建议顺序为：

1. 记录J1~J4对应LF/RF/LR/RR。
2. 记录正PWM时各编码器计数正负，建立独立encoder direction。
3. 只做低速开环转动与计数核对。
4. 核对每转计数、减速比和采样周期。
5. 最后才启用单轮速度PID，并保留PWM限幅和失控停止条件。

本阶段不初始化TIM1/TIM3/TIM4/TIM8，不读取编码器，也不启动PID。

## 9. 与四舵机资源冲突检查

- 电机使用TIM2及PA15/PB3/PB10/PB11。
- 舵机使用TIM5及PA0/PA1/PC5/PB0。
- 电机方向/STBY使用PC13/PC14/PC15/PC0/PC1/PC8/PC9/PB12/PB13/PB14。
- 上述GPIO与四舵机GPIO没有重叠。
- PB5没有修改，继续保留给TIM3_CH2编码器。
- `main.c`不启动FreeRTOS、编码器、PID、遥控、ROS或飞控通信。
- 电机测试循环中没有其他活动代码修改TIM2的CCR。

编译过程不会自动烧录。
