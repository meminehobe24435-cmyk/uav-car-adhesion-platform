# 飞控到小车 USART2 通信（第一阶段诊断版）

## 1. 当前验证目标

当前数据链为：

```text
COMTool -> USB-TTL -> 底板 RX2/USART2_RX
        -> flight_comm -> 协议、字段和 CRC8 校验
        -> 底板 TX2/USART2_TX -> USB-TTL -> COMTool
```

本阶段只验证串口双向通信：

- 合法完整帧返回 `OK\r\n`。
- 帧头正确但 CRC8 错误的候选帧返回 `CRC_ERR\r\n`。
- 实际电机输出由 `FLIGHT_COMM_ENABLE_CAR_OUTPUT=0` 强制关闭。
- `servo.c`、`steering.c`、`motor.c`、`car.c` 未修改。
- 未启动 FreeRTOS、编码器 PID、PS2、ROS 或 MAVLink。

修改前备份：

`D:\zhuomian\weite\无人机小车部分\backups\差速底盘_5_11_before_usart2_migration_20260812_151532`

## 2. 原理图和资源审计

原理图中 MCU 串口网络名为 `MCU_UART2_TX`、`MCU_UART2_RX`，并连接到板上 4Pin 接口：RX2、TX2、GND、5V。STM32F103RCT6 当前工程配置如下：

| 项目 | 配置 |
|---|---|
| 外设 | USART2 |
| TX | PA2 / USART2_TX |
| RX | PA3 / USART2_RX |
| 波特率 | 115200 |
| 格式 | 8N1，无流控 |
| RX DMA | DMA1_Channel6，Circular，High priority |
| TX DMA | DMA1_Channel7，Normal，Medium priority |
| USART2 IRQ | 已配置 |

USART2 在旧工程中并非完全没有源代码引用：`com_debug.c` 的 `printf` 重定向和 `vofa.c` 会向 `huart2` 发送数据；部分未启动任务也包含调试输出。但当前 `main.c` 没有启动 FreeRTOS、VOFA 或这些调试任务，因此运行时 USART2 可由 `flight_comm` 独占。后续启用调试/VOFA 前必须迁移它们的输出端口，否则 ASCII 调试内容会混入飞控协议链路。

资源检查：

| 模块 | 资源 | 与 USART2 结论 |
|---|---|---|
| 四舵机 | TIM5，PA0/PA1，PC5/PB0 | 不冲突 |
| 四电机 | TIM2，PA15/PB3/PB10/PB11 | 不冲突 |
| 编码器预留 | TIM1/TIM3/TIM4/TIM8 | 不冲突 |
| PS2 | UART5，PC12/PD2 | 独立外设，当前不初始化，不影响 USART2 |
| 旧飞控后端 | UART4，PC10/PC11 | 当前不初始化，已由 USART2 替代 |

## 3. USB-TTL 接线

底板保持由自身电池供电：

| USB-TTL | CAR-MOTOR-V1.2S 4Pin 接口 |
|---|---|
| TXD | RX2（PA3 / USART2_RX） |
| RXD | TX2（PA2 / USART2_TX） |
| GND | GND |
| 5V | 不连接 |

USB-TTL 必须使用 3.3V TTL 逻辑电平。禁止把 RS-232 电平直接接到 STM32。三根信号线必须共地，TX/RX 必须交叉。

## 4. 固定 11 字节协议

| 字节 | 字段 | 说明 |
|---|---|---|
| 0 | Header0 | `0xAA` |
| 1 | Header1 | `0x55` |
| 2 | Version | 固定 `0x01` |
| 3 | Enable | `0` 或 `1` |
| 4 | Mode | `0` 普通转向，`1` 蟹行 |
| 5..6 | Throttle | int16，大端，限制到 -1000..1000 |
| 7..8 | Steering | int16，大端，限制到 -1000..1000 |
| 9 | Sequence | 0..255 |
| 10 | CRC8 | Byte0..Byte9 的 CRC |

解析器从任意字节位置重新搜索 `AA 55`。CRC、version、enable 或 mode 非法的帧不会更新控制命令。一个丢字节不会永久破坏后续帧同步。

## 5. CRC8

采用 CRC-8/ATM：

- Polynomial：`0x07`
- Init：`0x00`
- RefIn/RefOut：False
- XorOut：`0x00`
- 计算范围：Byte0..Byte9，共 10 字节
- 标准向量 `123456789` 的结果：`0xF4`

只有 CRC 错误会返回 `CRC_ERR\r\n`；非法 version/mode/enable 会静默拒绝，防止错误数据进入控制层。

## 6. DMA 接收机制

`flight_comm_init()` 在 `MX_DMA_Init()` 和 `MX_USART2_UART_Init()` 之后调用，通过：

```c
HAL_UART_Receive_DMA(&huart2, flight_dma_rx_buffer, 128U);
```

启动 DMA1_Channel6 环形接收。`flight_comm_update()`读取 DMA 剩余计数并处理新增字节。DMA1_Channel6、DMA1_Channel7 和 USART2 的 IRQ 入口均已连接到对应 HAL 处理函数。

诊断回传使用 USART2 阻塞 TX，单条最多 9 字节，在 115200 baud 下耗时小于 1 ms，不停止 RX DMA。

## 7. 500 ms 失联保护和本阶段电机锁定

上电默认：

```text
connected = 0
enable = 0
throttle = 0
steering = 0
car_stop()
```

合法帧才会设置 `connected=1` 并更新 `last_valid_packet_ms`。超过 500 ms 未收到合法帧后，通信层清零命令、设置 `enable=0` 并调用 `car_stop()`；`rc_control` 还保留自己的第二层失联保护。

本阶段 `app/flight_comm.h` 中：

```c
#define FLIGHT_COMM_ENABLE_CAR_OUTPUT 0
```

因此合法帧仍会解析并回复 `OK`，但传给 `rc_control` 的始终是零油门、零转向和 `enable=0`。完成串口诊断并准备架空车轮进行运动测试时，才可单独评审后改为 `1`。

## 8. rc_control 后端

`flight_comm.c` 保留唯一的强定义 `rc_backend_read()`：

```text
USART2 -> flight_comm -> rc_backend_read -> rc_control -> car
```

以后切换 PS2 或其他接收机时，可将 `FLIGHT_COMM_PROVIDE_RC_BACKEND` 改为 0，并由新后端提供唯一的 `rc_backend_read()`；`RcCommand` 和 `car` 接口无需改变。

## 9. COMTool 验证方法

COMTool 设置：

- 端口：CH340 对应的 COM 口（当前为 COM3）
- 115200、8 data bits、None parity、1 stop bit、None flow control
- 接收显示：ASCII（用于直接看到 `OK`/`CRC_ERR`）
- 发送格式：HEX
- 关闭“定时发送”和“自动添加 CRLF/换行”

先关闭可能占用 COM3 的 Python、Keil 串口窗口或其他串口工具，再点击“打开”。

安全合法帧（enable=0、mode=0、throttle=0、steering=0、sequence=0）在发送框输入：

```text
AA 55 01 00 00 00 00 00 00 00 20
```

发送后应收到：

```text
OK
```

CRC 错误测试，把末字节改为 `00`：

```text
AA 55 01 00 00 00 00 00 00 00 00
```

应收到：

```text
CRC_ERR
```

若提示 `PermissionError(13)`，说明 COM3 被其他程序占用，而不是波特率或协议错误。关闭占用程序，拔插 CH340，刷新端口后重试。

## 10. 换成真实飞控

真实飞控串口需要设置为 115200、8N1、3.3V TTL：飞控 TX 接底板 RX2，飞控 RX 接底板 TX2，双方 GND 共地。飞控必须周期发送同一 11 字节协议，并把遥控失效状态映射为 `enable=0`。建议周期 100 ms；STM32 保留 500 ms 超时停车。

USB-TTL 成功只能证明底板 USART2、协议和 PC 测试链路正常；真实飞控还需单独验证串口电平、端口配置、字节序、CRC、发送周期和飞控端 failsafe。

## 11. 后续升级 MAVLink

自定义协议稳定后，可新增独立 MAVLink backend，并继续输出统一的 `RcCommand`。MAVLink heartbeat 和控制消息都必须纳入 500 ms 超时策略；不应把 MAVLink 解析写入 `car.c`，也不能同时启用两个具有控制权的强定义 backend。
