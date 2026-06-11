# 支持 Modbus-RTU 协议通信的充电系统（Qt/C++）

## 环境

- Qt 5.14.2 或兼容 Qt 5 Widgets
- MinGW 7.3 64-bit 或 Qt Creator 对应编译套件
- C++17

## 打开方式

用 Qt Creator 打开 `ChargingModbusQt.pro`，选择 Desktop Qt 5.14.2 MinGW 64-bit 套件，构建并运行。

## 命令行构建

```powershell
set PATH=C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin;C:\Qt\Qt5.14.2\Tools\mingw730_64\bin;%PATH%
qmake ChargingModbusQt.pro
mingw32-make
release\ChargingModbusQt.exe
```

## 协议自测

```powershell
qmake ProtocolSelfTest.pro
mingw32-make
release\ProtocolSelfTest.exe
```

## 功能

- Qt Widgets 自适应窗口，控制器、采集器、协议监视器三面板可随窗口伸缩。
- 标准 Modbus-RTU 帧：Address + PDU + CRC-16/MODBUS，CRC 低字节在前。
- 支持 `0x04` 读多个寄存器、`0x05` 写单线圈、`0x16` 写多个寄存器。
- 支持异常响应、BCD 学号刷卡、恒流/恒压电池模型、满电停机和过压/过流/高温停机。
- 支持协议日志、帧解释、数据曲线、日志导出、数据导出、会话账单导出。
- 运行摘要增加预计充满时间和风险等级，历史 CSV 增加 SOC、安全裕量和风险标记。
- 支持按钮防误触、曲线悬停查看采样值、报警/满电/未插枪诊断建议。
- 支持虚拟链路、真实串口控制器端、真实串口采集器响应端切换，可用虚拟串口对完成双端联调。
- 支持充电流程状态图、Modbus 帧彩色结构、安全裕量仪表和会话柱状图。
