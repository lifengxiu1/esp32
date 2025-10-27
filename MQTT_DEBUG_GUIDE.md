# ESP32-S3 MQTT调试指南

## 功能概述

本项目实现了完整的MQTT控制协议，支持：

### 下行命令（控制命令，主题：`rgb/ctrl`）
- **点亮单个LED**：指定索引和倒计时时间
- **关闭单个LED**：指定索引
- **全部点亮**：无倒计时，持续点亮
- **全部关闭**：关闭所有LED

### 上行回执（实时反馈，主题：`rgb/ack`）
- **命令执行确认**：每次执行命令后立即上报
- **周期性状态更新**：每秒上报一次全量状态

## 命令格式

### 下行命令示例

```json
// 点亮第1盏灯，30秒倒计时（索引0表示第1盏）
{
  "cmd": "set",
  "effect": "solid", 
  "pixel": 0,
  "duration_ms": 30000
}

// 关闭第1盏灯
{
  "cmd": "set",
  "effect": "off",
  "pixel": 0
}

// 全部点亮（无倒计时）
{
  "cmd": "set",
  "effect": "solid"
}

// 全部关闭
{
  "cmd": "set", 
  "effect": "off"
}
```

### 上行回执格式

```json
{
  "ok": true,
  "reason": "ok",
  "ts": 1700000000,
  "lights": [
    {"id": 1, "on": 1, "remain_ms": 28000},
    {"id": 2, "on": 0, "remain_ms": null},
    ...
    {"id": 74, "on": 1, "remain_ms": null}
  ]
}
```

## 调试工具使用

### 1. 自动测试脚本

运行完整的自动化测试：

```bash
python mqtt_test.py
```

这个脚本会自动测试所有功能：
- 点亮第1盏灯（30秒倒计时）
- 关闭第1盏灯
- 全部点亮
- 全部关闭
- 点亮第10盏灯（60秒倒计时）

### 2. 交互式命令行工具

运行交互式测试：

```bash
python mqtt_simple_test.py
```

选择菜单选项进行测试：
- 1: 点亮第1盏灯（30秒倒计时）
- 2: 关闭第1盏灯
- 3: 全部点亮（无倒计时）
- 4: 全部关闭
- 5: 点亮第10盏灯（60秒倒计时）

### 3. 手动MQTT命令

使用`mosquitto_pub`命令手动测试：

```bash
# 点亮第1盏灯，30秒倒计时
mosquitto_pub -h 192.168.158.1 -t rgb/ctrl -m '{"cmd":"set","effect":"solid","pixel":0,"duration_ms":30000}'

# 关闭第1盏灯
mosquitto_pub -h 192.168.158.1 -t rgb/ctrl -m '{"cmd":"set","effect":"off","pixel":0}'

# 全部点亮
mosquitto_pub -h 192.168.158.1 -t rgb/ctrl -m '{"cmd":"set","effect":"solid"}'

# 全部关闭
mosquitto_pub -h 192.168.158.1 -t rgb/ctrl -m '{"cmd":"set","effect":"off"}'
```

监听状态反馈：

```bash
mosquitto_sub -h 192.168.158.1 -t rgb/ack
```

## 依赖安装

确保安装了Python MQTT客户端库：

```bash
pip install paho-mqtt
```

## 设备烧录

确保ESP32-S3设备已烧录最新固件：

```bash
idf.py flash
```

## 网络配置

- **MQTT服务器**：`192.168.158.1:1883`
- **控制主题**：`rgb/ctrl`
- **状态主题**：`rgb/ack`

确保ESP32-S3设备与MQTT服务器在同一网络下。

## 调试技巧

1. **先测试连接**：运行测试脚本查看是否能收到设备状态
2. **观察TFT显示屏**：设备状态会实时显示在屏幕上
3. **查看串口日志**：使用`idf.py monitor`查看设备调试信息
4. **检查网络连接**：确保设备已连接到WiFi

## 故障排除

### 常见问题

1. **连接失败**：检查MQTT服务器IP地址和端口
2. **无响应**：检查设备是否正常运行，WiFi是否连接
3. **命令无效**：检查JSON格式是否正确，字段名称是否匹配

### 日志查看

设备串口输出包含详细的调试信息：

```bash
idf.py monitor
```

查看MQTT连接状态、命令处理过程和错误信息。