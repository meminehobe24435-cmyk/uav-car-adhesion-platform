# PS2 基础遥控小车说明

## 1. 工程与备份

开发工程：

```text
D:\zhuomian\weite\无人机小车部分\差速底盘 源码包 (2)\差速底盘 源码包\差速底盘 源码包\差速底盘_5_11
```

修改前完整备份：

```text
D:\zhuomian\weite\无人机小车部分\差速底盘 源码包 (2)\差速底盘 源码包\差速底盘 源码包\差速底盘_5_11_before_ps2_basic_20260814_100956
```

备份与修改前开发工程均为 1237 个文件、133488824 字节；关键文件 SHA256 已逐一核对一致。备份目录未作任何修改。

## 2. 修改文件

- `Src/main.c`：上电安全初始化、PS2更新、500ms失联保护、L1/R1/START处理、固定低速和固定转向控制、200ms调试输出。
- `bsp/ps2_usart.h`：增加实测确认的Byte6按键位掩码和原始按键字段。
- `bsp/ps2_usart5.c`：保留厂家UART5和文本协议解析，修正面键名称，将Byte6按位解析，支持同时按键。
- `PS2_BASIC_CONTROL.md`：本说明文件。

没有修改：

```text
bsp/car.c
bsp/motor.c
bsp/servo.c
bsp/steering.c
```

## 3. 控制链与数据映射

```text
PS2手柄
→ USB无线接收器
→ CH559 USB Host
→ UART5（PC12/PD2，57600 8N1）
→ ps2_parse_data()
→ Ps2Command
→ car接口
→ motor + steering
```

CH559有效帧使用Report ID `0x01`；持续出现但不活动的 `0x02` 帧不会刷新连接时间。

| 字段 | 功能 | 本程序处理 |
|---|---|---|
| Byte3 | 左摇杆X，左减小、右增大 | 转换为 `lx=-128~127` |
| Byte4 | 左摇杆Y，上减小、下增大 | 转换为 `ly=-128~127` |
| Byte6 bit0 | L1 | 持续按住才允许运动 |
| Byte6 bit1 | R1 | 按住期间立即停车 |
| Byte6 bit5 | START | 停车并回正 |

Byte6完整实测位定义：

```text
bit0 L1
bit1 R1
bit2 L2
bit3 R2
bit4 SELECT
bit5 START
bit6 L3
bit7 R3
```

旧诊断中把 `x4F`标成R2的结果不再使用；`x4F`实际属于Byte5面键，R2是Byte6 bit3。

## 4. 安全按键

### L1安全使能

- L1必须持续按住。
- 未按L1时，无论摇杆在哪里都执行 `car_stop()`。
- 松开L1后，主循环在下一个10ms控制周期停车。

### R1急停

- R1优先于L1和摇杆命令。
- R1按住期间始终执行 `car_stop()`。
- 本极简版本不锁存；R1释放后仍需L1按住才可能运动。

### START回正

- START按下时先执行 `car_stop()`。
- START按下沿调用一次 `steering_center()`，避免按住按键反复执行舵机错峰更新。
- START按住期间电机始终停止。

## 5. 摇杆死区与固定命令

摇杆中位为128，解析后中位为0。死区为 `±15`：

```text
原始值113~143 → 中位
ly < -15       → 前进
ly > +15       → 后退
lx < -15       → 左转（+10°）
lx > +15       → 右转（-10°）
```

组合命令直接调用现有car接口：

| 摇杆命令 | car调用 |
|---|---|
| 前进 | `car_forward(10% PWM)` |
| 后退 | `car_backward(10% PWM)` |
| 仅左/右 | `car_stop()`后 `car_turn(±10°, 0)` |
| 前进/后退 + 左/右 | `car_turn(±10°, ±10% PWM)` |
| 回中 | `car_stop()` |

PS2代码不直接访问GPIO、TIM CCR、TB6612或舵机PWM。

## 6. 第一次测试限制

- TIM2 ARR=99，PWM满量程为100计数。
- `PS2_MOTOR_TEST_PERCENT=10`，所以第一次输出为PWM=10，即10%。
- `PS2_STEERING_TEST_ANGLE_DEG=10.0F`，所以最大转向为±10°。
- `motor_config[].pwm_max`原有30%上限仍然有效，但本程序只请求10%。
- 舵机最终仍受各自 `ServoConfig.min_us/max_us` 限幅。

## 7. 500ms失联保护

只有成功解析完整 `HUB0_Joystick data:` 且Report ID为 `0x01` 的帧，才会刷新 `last_valid_frame_ms`。

超过500ms没有合法 `0x01` 帧：

```text
connected = 0
L1/R1/START清零
lx/ly清零
car_stop()
```

因此遥控器关机、USB接收器拔出、CH559停止输出或UART5停止更新，都不会维持旧油门。

## 8. 上电与运行状态

主程序只启动：

```text
GPIO
TIM2（四电机PWM）
TIM5（四舵机PWM）
USART2（200ms调试输出）
UART5（CH559 PS2接收）
```

不调用FreeRTOS、不初始化编码器、不启动PID、不调用flight_comm，也不运行motor/servo/steering测试循环。

`car_init()`内部复用已验证的 `servo_init()` 和 `motor_init()`。上电随后执行停车与回正，必须等待合法PS2帧且持续按住L1才允许运动。

USART2每200ms最多输出一次：

```text
PS2=CONNECTED LX=... LY=... L1=... R1=... START=... CMD=...
```

CMD只可能是：

```text
STOP
FORWARD
BACKWARD
LEFT
RIGHT
FORWARD_LEFT
FORWARD_RIGHT
BACKWARD_LEFT
BACKWARD_RIGHT
```

## 9. 第一次架空测试步骤

1. 将四个车轮可靠架空，确保任何方向旋转都不会接触地面。
2. 检查舵机、电机、电池和PS2接收器接线后给小车上电。
3. 打开PS2并等待与USB无线接收器建立连接。
4. 不按L1，推动左摇杆：四个电机必须不动。
5. 持续按住L1，轻推左摇杆向上：四轮应以10%固定PWM前进。
6. 松开L1：四轮应立即停止。
7. 持续按住L1并向下推：四轮应以10%固定PWM后退。
8. 持续按住L1并向左推：舵机应转向车辆左侧，最大10°。
9. 持续按住L1并向右推：舵机应转向车辆右侧，最大10°。
10. 测试L1+上+左、L1+上+右、L1+下+左、L1+下+右组合。
11. 按START：电机必须停车，四轮必须回各自机械中位。
12. 运行时按住R1：电机必须停车。
13. 让车轮低速运行时关闭PS2；必须在500ms内停车。
14. 拔出USB无线接收器，再确认必须在500ms内停车。

只有上述方向和保护全部正确后，才允许将车轮落地。当前版本禁止提高PWM或转向角。

## 10. 方向调整

如果摇杆向上但整车后退，不改变PS2层的“向上=前进”语义；在 `bsp/motor.c` 的 `motor_config[MOTOR_COUNT]` 中调整对应电机 `direction`（`+1/-1`）。

如果摇杆向左但轮子实际向右，先核对 `steering_turn(+angle)` 的车辆坐标语义，再在 `bsp/servo.c` 的 `servo_config[SERVO_COUNT]` 中调整对应舵机 `direction`。禁止在PS2层针对单个轮子增加正负反转补丁。

## 11. 编译与烧录边界

Keil工程：

```text
MDK-ARM\1_template_led.uvprojx
```

本次仅执行Keil全量重建，不自动烧录。烧录前由操作人员确认车轮已经架空。
