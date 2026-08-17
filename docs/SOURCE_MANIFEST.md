# 源文件清单与公开边界

整理日期：2026-08-17

## 原始研发目录

本次只读审计并选择性整理：

- `D:\zhuomian\weite\无人机小车部分`
- `D:\zhuomian\weite\无人机攀附`

GitHub 仓库是独立副本；没有移动、清理或重置原始研发目录。

## 小车源码基线

选用的当前工作版本：

```text
差速底盘_5_11_PS2_NORMAL_TURN90_20260814_143958
```

收录：

- `app/`、`bsp/`、`com/`、`Drivers/`、`Inc/`、`Middlewares/`、`Src/`；
- Keil `uvprojx`、启动文件、RTE/DebugConfig；
- CubeMX `.ioc` 和 `.mxproject`；
- 当前相关 Markdown 说明和 Python UART 工具；
- 当前发布 HEX 与对应全量构建日志；
- CAR-MOTOR-V1.2S 原理图。

未收录：Keil Objects/Listings、AXF/BIN、中间目标文件、IDE 用户文件、Python 缓存和多份历史生成 HEX。

当前发布 HEX：

```text
car/releases/PS2_NORMAL_TURN90_SPEED30_60_80/1_template_led.hex
SHA256 ABE8271A1E9C588CF2D62D08F95AE26E29682AC93F7E971D0104B44E42962D17
```

## 飞控源码基线

只收录项目可明确归属的 PX4 覆盖文件：

- `height_commander` 模块、参数、Kconfig/CMake；
- `mylink_bridge` 模块、参数、Kconfig/CMake；
- MicoAir H743-v2、PX4 FMU-v6x、SITL 的三个板级配置；
- WIT F722 飞控参考 PDF。

记录的 PX4 上游：

```text
https://github.com/PX4/PX4-Autopilot.git
6388739efb068d72677f6a5777742e30aefa21a6
```

WIT F722 参考 PDF：

```text
adhesion/hardware/reference/WLKJ2026002UAV-WIT_F722-V3.2.pdf
SHA256 C989F57B3606CDEBE6CB7F9B067D87D8FF0070C19AB2242BD468B45B549BE584
```

## 没有上传的内容

| 内容 | 原因 |
|---|---|
| 合同、付款、人员分工、内部沟通稿 | 公开仓库不应包含商务或人员信息 |
| `PX4-test.7z`、`Nxt-FC-main.zip` 和其他源码压缩包 | 大体积重复归档，不利于审计和 GitHub 限制 |
| 完整本地 `PX4-test` | 上游仓库且当前工作树大量缺失/脏改动；不能冒充干净项目源码 |
| Nxt-FC、Mini UGV 完整第三方仓库/固件/模型 | 第三方许可与再分发边界不清，只保留来源和自研覆盖层 |
| BOM/采购表的供应商、价格和内部字段 | 不是代码复现所必需，可能含非公开业务信息 |
| 多份 DOCX/PDF/TXT 历史报告 | 技术结论已去敏整合进当前 Markdown 文档，避免版本互相矛盾 |
| PX4 build、Keil Objects/Listings、缓存 | 可再生成且体积大 |

“全部上传”在本仓库中指全部当前自研核心代码、必要工程配置、可追溯发布固件和公开技术说明；不指把数 GB 的第三方仓库、缓存、压缩包或敏感业务资料原样发布。

## 第三方与许可

- PX4 自定义文件保留原文件头；完整 PX4 应从官方上游获取；
- STM32 HAL、CMSIS、FreeRTOS 保留随工程原有的许可证和版权声明；
- Nxt-FC 参考来源：`https://github.com/Peize-Liu/Nxt-FC`；
- 本仓库没有对所有第三方内容擅自增加统一开源许可证。

## 证据口径

- 已收录：文件存在于仓库；
- 已编译：有与该源码版本对应的构建日志；
- 已实测：有用户明确观察、测试记录或可复核测量证据；
- 待验证：代码或方案存在，但缺少当前版本的硬件/飞行证据。

这四种状态不能互相替代。
