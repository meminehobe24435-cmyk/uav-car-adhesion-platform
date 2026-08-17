# 技术文档图片占位与命名规则

技术开发文档中的图片由后续实物测试人员补充。请优先使用 JPG/PNG，单张建议控制在 5 MB 以内，不要上传包含人员身份、客户资料、Wi-Fi 密码、串口序列号或其他敏感信息的截图。

| 编号 | 建议文件名 | 内容 |
|---|---|---|
| P01 | `P01_system_overview.jpg` | 整机总览和子系统标注 |
| P02 | `P02_flight_controller_actual.jpg` | 实际飞控正反面、型号和版本 |
| P03 | `P03_car_board_pinout.jpg` | 小车控制板接口总览 |
| P04 | `P04_ps2_controls.jpg` | PS2 按键功能标注 |
| P05 | `P05_flight_to_car_uart.jpg` | 飞控—车控 TX/RX/GND 接线 |
| P06 | `P06_car_bench_test.jpg` | 小车车轮架空测试 |
| P07 | `P07_motor_channels.jpg` | TB6612 与 J1~J4 线束 |
| P08 | `P08_servo_connectors.jpg` | 舵机接口和三线方向 |
| P09 | `P09_uart_logic_capture.png` | UART 协议逻辑分析仪截图 |
| P10 | `P10_servo_pwm_scope.png` | 四路 50 Hz PWM 示波器截图 |
| P11 | `P11_flight_parameters.png` | 飞控参数与固件目标截图 |
| P12 | `P12_protected_test_rig.jpg` | 防护网/压紧工装 |

补图后，把技术文档中对应的占位引用替换为：

```markdown
![P01 整机总览](images/P01_system_overview.jpg)
```

每张测试图旁应注明日期、固件 Git commit、硬件版本、供电条件和观察结论。照片只能证明画面中可观察到的事实，不能单独替代构建日志、示波器数据或飞行日志。
