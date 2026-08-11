# 源文件清单与公开边界

## 原始目录

本次整理只读审计了：

- `D:\zhuomian\weite\无人机小车部分`
- `D:\zhuomian\weite\无人机攀附`

原目录没有被移动、清理或重置；整合仓库是单独副本。

## 已收录

- 当前 STM32F103 小车完整可编译源码、Keil 工程和 CubeMX 配置；
- `SERVO_CALIBRATION.md`、`MOTOR_TEST.md`；
- CAR-MOTOR-V1.2S 原理图；
- 最近一次电机测试版本的 Keil 构建日志；
- PX4 `height_commander`、`mylink_bridge` 自定义模块；
- 三个与自定义模块直接相关的 `default.px4board` 配置；
- 基于现有技术报告和代码整理出的公开版进度/代码说明。

## 未收录及原因

| 内容 | 原因 |
|---|---|
| 合同、付款表述、人员任务分工和内部沟通稿 | 公开仓库不应包含商务与人员信息 |
| DOCX/PDF 的重复报告版本和渲染中间件 | Markdown 已覆盖技术结论，避免重复和体积膨胀 |
| `PX4-test.7z`、`Nxt-FC-main.zip`、小车源码压缩包 | 大体积归档，可由源码或上游重建 |
| 完整 `PX4-test` | 上游仓库且工作树大量脏改动；只提取项目自定义覆盖层 |
| `Nxt-FC-main` 固件和子模块 | 第三方项目，保留来源链接而不重复再发布二进制 |
| Mini UGV 厂商/参考整包、三维模型和固件 | 许可边界不清且包含大量生成物，仅作为内部参考 |
| Keil Objects/Listings、AXF/HEX/BIN、PX4 build | 生成产物，不应进入源码仓库 |
| 修改过但无法归因的 PX4 文件与子模块状态 | 不能把上游或工具生成变化冒充项目代码 |

## 第三方来源

- PX4-Autopilot：`https://github.com/PX4/PX4-Autopilot.git`，记录基线 `6388739efb068d72677f6a5777742e30aefa21a6`。
- Nxt-FC DUAL 参考：`https://github.com/Peize-Liu/Nxt-FC`（本地副本的子模块配置指向 Peize-Liu 的 PX4 与硬件仓库）。
- STM32 HAL、CMSIS、FreeRTOS：随当前 Keil 工程保留原文件与许可证声明。

## 验证口径

“已收录”只表示文件被整理进仓库；“已编译”必须有构建日志；“已实测”必须有用户确认或可复核硬件证据。脚本、构建目录或二进制文件名本身不作为测试成功证明。

