# 无人机正压攀附与四舵轮小车项目

本仓库把两个本地研发目录中可以公开、可以复用的代码和技术结论整理到一个可审计的工程入口：

- `car/`：STM32F103RCT6 四舵轮小车控制工程；
- `adhesion/`：PX4 自定义模块与板级配置覆盖层；
- `docs/`：截至 2026-08-11 的项目进度、代码地图、证据边界和源文件清单。

## 当前结论

小车侧已经完成四舵机独立控制、机械零位标定和基础四轮转向接口；对应 Keil 工程最近一次留存的全量构建记录为 `0 Error(s), 0 Warning(s)`。四直流电机独立驱动层也已进入工程，当前默认程序是只对 `MOTOR_1 / J1` 做正反转低占空比测试，四个电机与车辆 LF/RF/LR/RR 的物理对应关系尚待实车确认。

攀附飞控侧已经形成总体架构和两个 PX4 实验模块：`height_commander` 用于开关触发的相对高度变化，`mylink_bridge` 用于串口文本命令桥接。当前目录能确认源码和配置已就位，但本次整理没有找到可作为证据的最终 PX4 固件或完整测试日志，因此不宣称已经完成飞行验证。

详细状态见 [项目进度](docs/PROJECT_STATUS.md)，代码入口见 [代码地图](docs/CODE_MAP.md)。

## 仓库结构

```text
.
├─ car/
│  ├─ firmware/stm32f103-car-controller/  # 可直接用 Keil 打开的 STM32 工程
│  └─ hardware/                            # CAR-MOTOR-V1.2S 原理图
├─ adhesion/
│  └─ px4-overlay/                         # 叠加到指定 PX4 基线的自定义代码
└─ docs/
   ├─ PROJECT_STATUS.md
   ├─ CODE_MAP.md
   ├─ SOURCE_MANIFEST.md
   └─ evidence/                            # 可复核构建记录
```

## 快速入口

- 小车工程说明：[car/README.md](car/README.md)
- 舵机标定：[SERVO_CALIBRATION.md](car/firmware/stm32f103-car-controller/SERVO_CALIBRATION.md)
- 电机测试：[MOTOR_TEST.md](car/firmware/stm32f103-car-controller/MOTOR_TEST.md)
- PX4 覆盖层说明：[adhesion/README.md](adhesion/README.md)
- 本次收录与排除范围：[SOURCE_MANIFEST.md](docs/SOURCE_MANIFEST.md)

## 安全提示

- 当前小车 `main.c` 上电后会保持舵机回中，并进入 `MOTOR_1 / J1` 的 `+20% / -20%` 循环测试。刷写前必须架空车轮并准备断电。
- `mylink_bridge` 是实验代码，能够请求解锁并直接发布电机输出；当前协议没有认证、校验和完整的输入限幅，禁止直接用于实机飞行。
- 本仓库没有自动烧录、自动解锁或自动部署流程。

## 许可边界

本仓库没有为整合项目擅自新增统一开源许可证。STM32 HAL、FreeRTOS、PX4 覆盖文件及其他第三方组件继续受各自文件头或原项目许可证约束。来源和基线见 [SOURCE_MANIFEST.md](docs/SOURCE_MANIFEST.md)。

