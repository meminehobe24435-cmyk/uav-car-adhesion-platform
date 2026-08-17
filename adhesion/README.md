# 攀附飞控 PX4 覆盖层

本目录只保存项目自定义 PX4 模块和直接相关的板级配置，不复制完整上游仓库。

## 基线

- 上游：`https://github.com/PX4/PX4-Autopilot.git`
- 记录 commit：`6388739efb068d72677f6a5777742e30aefa21a6`
- 主要源码目标：`micoair_h743-v2_default`
- 参考硬件 PDF：`hardware/reference/WLKJ2026002UAV-WIT_F722-V3.2.pdf`

参考 PDF 的 F722 板型与当前 MicoAir H743-v2 源码目标不是同一个可互换目标。正式使用前必须按实际装机板冻结硬件和固件目标。

## 自定义模块

### `height_commander`

RC 辅助开关触发相对高度变化：`IDLE → ASCENDING → HOLDING_HIGH → DESCENDING → IDLE`。默认 `HC_ENABLE=0`，`HC_DELTA=5 m`，`HC_CHANNEL=7`。

当前通道 6 和 7 都映射到 `aux2`，必须在实机前复核。模块没有 ToF、触点、压紧力和完整低电量脱离逻辑，因此不是完整攀附控制器。

### `mylink_bridge`

历史实验模块，读取 `TAKEOFF`、`T <value>`、`LAND`、`STOP` 文本，并可能请求解锁或发布直接电机输出。

该代码没有认证、CRC、心跳超时和可靠输入限幅，且会绕过常规车辆级控制，禁止在真实飞行中启用。它也不等于 STM32 端的 11 字节 CRC8 车辆协议。

## 板级覆盖

| 目标 | 自定义模块 |
|---|---|
| `micoair_h743-v2_default` | `height_commander` |
| `px4_fmu-v6x_default` | `mylink_bridge` |
| `px4_sitl_default` | 两者 |

## 使用方法

在干净的指定 PX4 commit 上，将 `px4-overlay/` 按相同路径覆盖，然后先执行 SITL，再执行对应硬件目标构建：

```bash
make px4_sitl_default
make micoair_h743-v2_default
```

当前仓库没有可复核的最终 PX4 构建通过日志或实飞日志，不能把“源码已收录”写成“飞控已验证”。详细说明见 [技术开发文档](../docs/TECHNICAL_DEVELOPMENT_DOCUMENT.md)。
