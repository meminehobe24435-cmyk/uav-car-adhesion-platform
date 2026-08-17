# 小车基础运动控制与遥控输入预留

## 1. 当前交付状态

- 已新增基础运动控制层 `bsp/car.h`、`bsp/car.c`。
- 已新增协议无关的遥控输入层 `app/rc_control.h`、`app/rc_control.c`。
- 主循环已经切换为安全的遥控轮询框架，不再执行电机或舵机测试循环。
- 当前没有选择具体接收机协议，默认后端不产生数据，因此上电状态固定为 `connected=0`、`enable=0`、四个电机停止。
- 未启动 FreeRTOS、编码器、PID、Ackermann、ROS、飞控通信或自动导航。
- Keil ARMCC 5.06 全量编译结果：`0 Error(s), 0 Warning(s)`。
- 本次只编译，没有自动烧录。

完整修改前备份：

`D:\zhuomian\weite\无人机小车部分\backups\差速底盘_5_11_before_car_rc_20260811_185303`

## 2. car 控制层结构

`car.c` 位于舵机转向层和电机底层之上：

```text
main.c / rc_control.c
        |
        v
      car.c
      /   \
steering.c motor.c
     |        |
  servo.c   TIM2 + TB6612
```

`car_init()` 的上电顺序为：

1. `servo_init()`；
2. `motor_init()`；
3. `steering_center()`；
4. `motor_stop_all()`。

电机底层仍由 `MotorConfig.direction` 统一处理各电机的物理安装正反方向，上层不针对某个轮子交换正负号。

## 3. 基础运动函数

| API | 行为 |
|---|---|
| `car_stop()` | 四路电机 PWM 全部置零，舵机保持当前角度；如需同时回正，随后调用 `steering_center()`。 |
| `car_forward(speed)` | 先回正四个舵机，再向四路电机发送相同的车辆坐标正速度。 |
| `car_backward(speed)` | 先回正四个舵机，再向四路电机发送相同的车辆坐标负速度。 |
| `car_turn(angle, speed)` | 调用基础四轮转向：前轴为 `angle`，后轴为 `-angle`，四个电机保持相同车辆坐标速度。 |
| `car_crab(angle, speed)` | 四个舵机使用相同车辆坐标角度，四个电机保持相同车辆坐标速度。 |

`speed` 是 TIM2 的原始 PWM 计数值，不是百分比。可使用 `motor_percent_to_pwm(percent)` 由百分比换算。

`car.c` 和现有 `steering.c` 都保留了 ±30° 上限。当前蟹行只能实现斜向行驶，不代表已经支持 90° 横向侧移。

为避免主循环反复执行 `steering.c` 中每路错开 100 ms 的舵机更新，`car.c` 会缓存最近一次转向模式和角度；只有模式或角度实际变化时才重新更新舵机。电机速度仍会在每次控制调用中刷新。

## 4. 遥控抽象层

统一命令结构：

```c
typedef struct
{
    int16_t throttle;   /* -1000 .. +1000 */
    int16_t steering;   /* -1000 .. +1000，正值向左 */
    uint8_t mode;       /* 0=四轮转向，1=蟹行 */
    uint8_t enable;     /* 0=禁止电机，1=允许运动 */
    uint8_t connected;  /* 0=失联，1=连接正常 */
} RcCommand;
```

已实现：

- `rc_init()`：清空命令、默认断联和禁止、立即 `car_stop()`。
- `rc_update()`：读取一帧新数据、限幅、更新时间戳并执行失联保护。
- `rc_get_command()`：返回当前安全命令的副本。
- `rc_backend_read()`：弱定义的协议适配钩子。具体 PS2/SBUS/IBUS/CRSF/UART 驱动只需提供同名强定义，不需要修改 `car.c`。

`rc_backend_read()` 只有在取得“一帧完整且新鲜的数据”时才能返回 1。重复返回旧缓存会破坏 500 ms 失联判断，禁止这样实现。

预留通道语义：

| 接收机通道 | 统一字段 |
|---|---|
| CH1 | `steering` |
| CH2 | `throttle` |
| CH5 | `mode` |
| CH6 | `enable` |

这些只是逻辑映射预留；当前未绑定任何具体接收机协议或 GPIO。

## 5. 当前主循环限制与映射

- 控制周期：10 ms。
- `throttle` 和 `steering` 都先限制在 -1000 到 +1000。
- 中心死区：-50 到 +50（包含边界）映射为 0。
- 最大电机输出：TIM2 满量程的 20%。当前 TIM2 `ARR=99`，满量程为 100 个计数，所以第一次遥控最大命令为 20。
- 最大转向角：±15°。
- `mode=0` 调用 `car_turn()`。
- `mode=1` 调用 `car_crab()`。
- 负油门会以负的车辆坐标速度传给四个电机，因此可以后退转向或后退蟹行。

## 6. 失联保护逻辑

以下任一条件成立时均立即调用 `car_stop()`：

- `enable == 0`；
- `connected == 0`；
- 从未收到有效连接帧；
- 距离最后一帧完整有效数据超过 500 ms。

进入失联状态后，还会把 `throttle`、`steering` 清零，并强制 `enable=0`、`connected=0`。系统只有收到新的、完整的、声明连接正常的协议帧后才能退出断联状态。

## 7. 现有遥控相关代码审计

工程中已经存在一套 PS2 转 UART 桥接代码：

- 文件：`bsp/ps2_usart5.c`、`bsp/ps2_usart.h`；
- 串口：UART5；
- 波特率：57600；
- TX：PC12；
- RX：PD2；
- 接收方式：逐字节中断；
- 输入格式：以 `HUB0_Joystick data:` 开头、后跟 8 个十六进制字节的 ASCII 行；
- 旧使用路径：`app/chassis_task.c` 将 `ps2.lx/ly` 映射到旧差速底盘结构。

注意事项：

1. 这不是 STM32 直接读取 PS2 手柄的 SPI 驱动，也不是 SBUS、PPM、IBUS 或 CRSF 驱动，而是针对已有外部桥接器文本格式的解析器。
2. 旧 `remote_stopped_flag` 按“非零摇杆/按键活动”刷新时间，摇杆长期保持中位时会被误认为停止，不能直接承担本阶段的 500 ms 链路心跳判断。
3. `app/remote_task.c` 实际执行 IMU 初始化/更新，并不是遥控器接收任务。
4. 未发现可直接复用的 SBUS、PPM、IBUS 或 CRSF 接收实现。
5. 主循环当前没有初始化 UART5，也没有启动旧 chassis/remote FreeRTOS 任务，因此不会与新抽象层并行控制电机。

## 8. 推荐使用的已有接口

如果实物仍使用工程原配的 PS2 转串口桥接器，优先复用 UART5：PC12/PD2、57600。后续需要新增一个 `rc_backend_read()` 适配文件，把每一帧有效 PS2 数据映射成 `RcCommand`，并为“每一帧有效数据”更新时间戳，不能沿用旧的非零活动计时方法。

如果以后使用普通航模接收机，应先确认具体协议和电气电平，再决定是否复用 UART5；SBUS 还需要确认反相与串口格式，不能仅修改波特率后直接连接。

## 9. 后续替换为无人机飞控 UART 控制

工程还配置了 UART4：

- TX：PC10；
- RX：PC11；
- 波特率：115200；
- RX 已配置 DMA 和串口中断。

建议未来把 UART4 作为飞控到小车的独立控制链路，保留 USART2 给调试/ROS/VOFA。替换步骤：

1. 定义带帧头、长度、序号、控制字段和 CRC 的二进制数据帧；
2. 用 UART4 DMA + 空闲线或明确长度方式收取完整帧；
3. 校验帧头、长度、范围、序号和 CRC；
4. 校验成功后在 UART4 适配文件的 `rc_backend_read()` 中输出 `RcCommand`；
5. 每一帧有效飞控数据刷新连接时间，飞控心跳中断超过 500 ms 自动停机；
6. 保持 `main.c`、`car.c`、`motor.c` 和 `steering.c` 不变。

飞控协议确认前，本工程不会自动启用 UART4 控制。

## 10. 资源与安全边界

- 电机继续使用 TIM2 四路 PWM 和两颗 TB6612，不改电机 PWM 资源。
- 舵机继续使用 TIM5 硬件/软件 PWM 组合，不改现有 `servo.c`、`steering.c` 实现。
- TIM1/TIM3/TIM4/TIM8 编码器资源未初始化、未修改。
- FreeRTOS 未启动。
- 没有编码器 PID、Ackermann、90° 横移、自动导航、ROS 或飞控通信。
- 本次编译输出为 `MDK-ARM/1_template_led/1_template_led.axf` 和 `1_template_led.hex`，但没有烧录到硬件。
