# 攀附飞控 PX4 覆盖层

这里不是 PX4 整仓副本，而是从本地工作树提取的项目自定义代码和必要配置。这样可以避免复制约 1.5 GB 的归档、构建目录、子模块和与项目无关的上游源码。

## 上游基线

- 上游：`https://github.com/PX4/PX4-Autopilot.git`
- 本地记录基线：`6388739efb068d72677f6a5777742e30aefa21a6`
- 目标板：`micoair_h743-v2_default`
- 本地工作树在整理时有大量未提交改动，因此这里只收录明确归属本项目的文件，不把原工作树状态包装成干净提交。

## 收录模块

### height_commander

基于 `manual_control_setpoint`、`vehicle_local_position` 和 `vehicle_status` 的状态机：

```text
IDLE -> ASCENDING -> HOLDING_HIGH -> DESCENDING -> IDLE
```

参数：

- `HC_ENABLE`：功能开关，默认 0；
- `HC_DELTA`：相对高度变化，默认 5 m；
- `HC_CHANNEL`：触发通道，默认 7。

该模块已加入 MicoAir H743-v2 和 SITL 的板级配置。现有代码中通道 6 与 7 都映射到 `aux2`，必须在实机使用前结合当前 PX4 消息定义核对。

### mylink_bridge

通过串口读取 `TAKEOFF`、`T <value>`、`LAND`、`STOP` 文本命令，并发布车辆命令或直接执行器控制。参数为 `MLB_ENABLE`、`MLB_PORT`、`MLB_BAUDRATE`。

它目前只在 SITL 和 `px4_fmu-v6x_default` 配置中启用，没有在 MicoAir H743-v2 配置中启用。代码未实现认证、帧校验、命令超时和完整的推力限幅，属于危险的实验原型，禁止直接用于实机。

## 应用覆盖层

在一个干净的 PX4 基线仓库中，将 `px4-overlay/` 下的文件按相同相对路径复制到仓库根目录。随后根据目标执行标准 PX4 构建：

```bash
make px4_sitl_default
make micoair_h743-v2_default
```

覆盖文件会替换三个 `default.px4board`。应用前应先检查上游版本差异；不要覆盖本地未提交改动。

## 当前证据边界

原目录中存在 `px4_sitl_default` 和 `micoair_h743-v2_default` 的 CMake 配置目录，SITL 模块列表包含两个自定义模块；但本次整理没有找到最终 `px4` 可执行文件、`.px4` 固件或完整的通过日志。因此当前状态应表述为“源码与构建配置已集成，编译和飞行验证待复核”。
