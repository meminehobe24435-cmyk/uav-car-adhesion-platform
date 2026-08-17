# 无人机正压攀附与四轮转向小车平台

本仓库整理了两个研发目录中可公开、可复现的核心代码：STM32F103 四舵机四电机小车固件，以及 PX4 攀附飞控自定义模块覆盖层。仓库同时提供一份面向后续开发、联调和交接的详细技术文档。

> 当前不是“整机已交付验证”的声明。小车基础执行机构和 PS2 链路已有实物验证；飞控自定义模块、飞控—车控 UART 和无人机/小车机械集成仍有不同程度的待编译、待台架或待实飞事项。具体证据边界见 [项目状态](docs/PROJECT_STATUS.md)。

## 快速入口

- [技术开发文档](docs/TECHNICAL_DEVELOPMENT_DOCUMENT.md)：总体架构、硬件、协议、代码讲解、安全设计、构建和测试计划。
- [代码地图](docs/CODE_MAP.md)：从功能定位到源码文件和调用链。
- [项目状态](docs/PROJECT_STATUS.md)：已实测、已实现待实测和未实现事项。
- [源文件清单](docs/SOURCE_MANIFEST.md)：本次收录、排除和第三方基线。
- [图片占位说明](docs/images/README.md)：后续补图的文件名、位置和内容要求。
- [小车固件说明](car/README.md)：Keil 工程、接线、PS2 操作和 HEX。
- [PX4 覆盖层说明](adhesion/README.md)：模块用途、适用板型和风险边界。

## 仓库结构

```text
.
├─ car/
│  ├─ firmware/stm32f103-car-controller/   # 当前完整 Keil/CubeMX 工程
│  ├─ hardware/                             # CAR-MOTOR-V1.2S 原理图
│  └─ releases/PS2_NORMAL_TURN90_SPEED30_60_80/
│     ├─ 1_template_led.hex                 # 当前可烧录固件
│     └─ keil_rebuild.log                   # 对应构建记录
├─ adhesion/
│  ├─ px4-overlay/                          # 自研 PX4 模块与板级覆盖配置
│  └─ hardware/reference/                   # 飞控硬件参考资料
└─ docs/
   ├─ TECHNICAL_DEVELOPMENT_DOCUMENT.md
   ├─ PROJECT_STATUS.md
   ├─ CODE_MAP.md
   ├─ SOURCE_MANIFEST.md
   └─ images/README.md
```

## 当前小车运行版本

上电执行 `car_init()`、`car_stop()` 和 `steering_center()`，然后进入 PS2 控制主循环。没有自动电机测试循环，没有启动 FreeRTOS、编码器 PID、ROS 或飞控通信。

| 手柄输入 | 当前动作 |
|---|---|
| L1 | 锁存直行 |
| A 或 Y | 锁存后退 |
| X | 锁存普通四轮左转并前进，目标角 `+90°` |
| B | 锁存普通四轮右转并前进，目标角 `-90°` |
| R1 | 立即停止并清除运动锁存 |
| L2 | 30% → 60% → 80% → 30% 单次按键沿切挡 |
| START | 停车并四轮回中 |

PS2 超过 500 ms 未收到合法的 CH559 `0x01` 报告后，固件停车、回中并清除锁存。这里的 `±90°` 是软件命令上限，实际 PWM 仍被各舵机 `min_us/max_us` 限制，不等价于机械转角已经完成 ±90° 实测。

## 飞控侧现状

PX4 覆盖层包含：

- `height_commander`：RC 辅助开关触发相对高度上升、保持和返回；
- `mylink_bridge`：实验性的 ASCII 串口命令到 PX4 执行器/车辆命令桥接；
- MicoAir H743-v2、FMU-v6x 和 SITL 的相关 `default.px4board` 覆盖文件。

`mylink_bridge` 目前没有 CRC、认证、命令超时和安全限幅，且包含直接执行器输出路径，只能用于代码研究，禁止直接用于真实飞行。STM32 侧另有 11 字节 CRC8 飞控协议模块，但当前 PS2 固件没有启动它；两者也不是同一协议，必须经过后续安全重构和联调才能组成正式链路。

## 构建与烧录

1. 用 Keil MDK 打开 `car/firmware/stm32f103-car-controller/MDK-ARM/1_template_led.uvprojx`。
2. 选择目标 `1_template_led`，执行 Rebuild。
3. 只在确认车辆架空、电机停止和接线正确后，通过 ST-Link/SWD 手动烧录。
4. 本仓库不会自动烧录、自动解锁飞控或自动启动电机。

## 安全边界

- 3S 电池、7.4 V 舵机电源和逻辑 5/3.3 V 必须按原理图分域供电并共地，禁止用飞控小电流 5 V 口直接带四个大扭矩舵机。
- 飞控型号尚未冻结：仓库源码目标、参考原理图和采购报告中出现过不同板型，接线和固件目标必须以实际装机板为准。
- GitHub 仓库未上传合同、人员分工、付款信息、私有归档、完整第三方 PX4 仓库和构建缓存。
- 第三方源码继续受原文件头和上游许可证约束；本仓库未擅自为全部内容增加统一许可证。
