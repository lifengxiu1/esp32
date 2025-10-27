# ESP32-S3 LED控制项目开发文档

## 项目概述

本项目是一个基于ESP32-S3的智能LED灯带控制系统，通过MQTT协议实现远程控制，支持74颗WS2812 LED灯带的精确控制，并配备TFT显示屏实时显示状态。

### 主要功能
- **MQTT远程控制**：通过JSON格式命令控制LED灯带
- **多效果支持**：支持常亮、闪烁、呼吸等多种LED效果
- **TFT状态显示**：实时显示74颗LED的状态和倒计时信息
- **WiFi自动连接**：支持WiFi STA模式自动连接和重连
- **定时控制**：支持LED定时关闭功能

## 项目结构

```
e:\esp32S3\hello_world\
├── main/                          # 主程序目录
│   ├── app_main.c                 # 应用程序入口
│   ├── net_mqtt.c                 # MQTT通信模块
│   ├── led/                       # LED控制模块
│   │   ├── led_ws2812.c           # WS2812硬件驱动
│   │   ├── led_effects.c          # LED效果控制
│   │   ├── led_ws2812.h           # LED驱动头文件
│   │   └── led_effects.h          # LED效果头文件
│   ├── display/                   # 显示模块
│   │   ├── status_list.c          # TFT状态列表显示
│   │   └── status_list.h          # 状态显示头文件
│   └── util/                      # 工具模块
│       └── color.h                # 颜色定义
├── managed_components/            # 组件管理器目录
│   └── espressif__led_strip/       # LED灯带驱动组件
├── build/                         # 构建输出目录
├── CMakeLists.txt                 # CMake构建配置
├── sdkconfig                      # ESP-IDF项目配置
├── dependencies.lock              # 依赖锁定文件
└── README.md                      # 项目说明文档
```

## 组件依赖关系

### 核心组件
- **ESP-IDF 5.4.2**：ESP32开发框架
- **espressif__led_strip 3.0.1**：LED灯带驱动组件
- **FreeRTOS**：实时操作系统
- **lwIP**：轻量级TCP/IP协议栈

### 硬件依赖
- **ESP32-S3**：主控芯片
- **WS2812 LED灯带**：74颗可寻址LED
- **TFT显示屏**：状态信息显示
- **WiFi网络**：MQTT通信

## MQTT通信协议

### 主题定义
- **控制主题**：`rgb/ctrl` - 发送控制命令
- **状态主题**：`rgb/ack` - 接收状态反馈

### 下行命令格式（控制命令）

#### 点亮单个LED
```json
{
  "cmd": "set",
  "effect": "solid", 
  "pixel": 0,
  "duration_ms": 30000
}
```

#### 关闭单个LED
```json
{
  "cmd": "set",
  "effect": "off",
  "pixel": 0
}
```

#### 全部点亮
```json
{
  "cmd": "set",
  "effect": "solid"
}
```

#### 全部关闭
```json
{
  "cmd": "set", 
  "effect": "off"
}
```

### 上行回执格式（状态反馈）

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

### 字段说明
- `cmd`：命令类型，固定为"set"
- `effect`：效果类型（"solid"、"off"）
- `pixel`：LED索引（0-73），可选，不指定时控制全部
- `duration_ms`：点亮时长（毫秒），可选
- `ok`：命令执行状态
- `reason`：执行结果描述
- `ts`：时间戳
- `lights`：所有LED状态数组

## LED灯带控制功能

### 硬件配置
- **GPIO引脚**：GPIO38（可配置）
- **LED数量**：74颗WS2812
- **颜色格式**：GRB格式
- **时钟频率**：10MHz

### 支持的效果

#### 1. 常亮效果（Solid）
- 支持单个LED或全部LED控制
- 可设置点亮时长
- 支持亮度调节（0-255）

#### 2. 关闭效果（Off）
- 支持单个LED或全部LED关闭
- 立即生效，无过渡效果

#### 3. 闪烁效果（Blink）
- 周期性开关LED
- 可调节闪烁频率
- 支持单个LED或全部LED

#### 4. 呼吸效果（Breathe）
- 平滑的亮度渐变效果
- 支持呼吸速度调节
- 仅支持全部LED控制

### API接口

#### 初始化函数
```c
void led_init_ws2812(int gpio, int count);
```

#### 效果控制函数
```c
void led_set_effect_off(void);
void led_set_effect_solid(rgb_color_t color, int pixel);
void led_set_effect_blink(rgb_color_t color, int speed_ms, int pixel);
void led_set_effect_breathe(rgb_color_t color, int speed_ms);
```

#### 亮度控制
```c
void led_set_brightness(int brightness);
int led_get_brightness(void);
```

#### 状态查询
```c
const char* led_effect_name(void);
rgb_color_t led_current_color(void);
```

## TFT显示屏功能

### 显示布局
- **竖屏显示**：320x240分辨率
- **两列布局**：每列显示多个LED状态
- **紧凑设计**：最大化信息密度

### 显示内容
- **LED编号**：1-74编号显示
- **状态指示**：红色方块表示点亮，白色表示关闭
- **倒计时**：mm:ss格式显示剩余时间
- **页码指示**：多页显示时的页码条

### 颜色主题
```c
#define COL_BG              RGB565(255,255,255)  // 白色背景
#define COL_TEXT            RGB565(0,0,0)         // 黑色文字
#define COL_STATUS_ON       RGB565(255,0,0)       // 红色点亮状态
#define COL_STATUS_OFF      RGB565(255,255,255)   // 白色关闭状态
```

### API接口

#### 初始化函数
```c
void status_list_init(int total_leds);
```

#### 状态设置函数
```c
void status_list_set_pixel(int idx0, int on, uint32_t duration_ms);
void status_list_set_all(int on, uint32_t duration_ms);
```

#### 状态查询函数
```c
int status_list_get(int idx0, int *on, uint32_t *remain_ms);
int status_list_total(void);
```

#### LED同步回调
```c
typedef void (*status_led_sync_cb_t)(int idx0, int on);
void status_list_set_led_sync_cb(status_led_sync_cb_t cb);
```

## 项目构建和部署指南

### 环境要求
- **ESP-IDF 5.4.2**：ESP32开发框架
- **Python 3.8+**：构建工具依赖
- **CMake 3.16+**：构建系统
- **Windows/Linux/macOS**：开发环境

### 构建步骤

#### 1. 环境设置
```bash
# 设置ESP-IDF环境变量
export IDF_PATH=/path/to/esp-idf
. $IDF_PATH/export.sh
```

#### 2. 项目配置
```bash
# 进入项目目录
cd e:/esp32S3/hello_world

# 配置项目（可选）
idf.py menuconfig
```

#### 3. 构建项目
```bash
# 清理构建缓存
idf.py fullclean

# 构建项目
idf.py build
```

#### 4. 烧录固件
```bash
# 烧录到ESP32-S3设备
idf.py -p COM3 flash
```

#### 5. 监控输出
```bash
# 查看串口输出
idf.py -p COM3 monitor
```

### 配置选项

#### WiFi配置
- `CONFIG_RGBMIN_WIFI_SSID`：WiFi名称
- `CONFIG_RGBMIN_WIFI_PASS`：WiFi密码

#### LED配置
- `CONFIG_RGBMIN_WS2812_GPIO`：LED控制GPIO（默认38）
- `CONFIG_RGBMIN_WS2812_COUNT`：LED数量（默认74）

#### MQTT配置
- `BROKER_HOST`：MQTT服务器地址
- `BROKER_PORT`：MQTT服务器端口（默认1883）

## 测试和调试指南

### 测试工具

#### 1. 自动测试脚本
```bash
# 运行完整测试套件
python mqtt_test.py
```

#### 2. 交互式测试工具
```bash
# 交互式测试
python mqtt_simple_test.py
```

#### 3. 手动MQTT测试
```bash
# 点亮第1盏灯，30秒倒计时
mosquitto_pub -h 192.168.158.1 -t rgb/ctrl -m '{"cmd":"set","effect":"solid","pixel":0,"duration_ms":30000}'

# 监听状态反馈
mosquitto_sub -h 192.168.158.1 -t rgb/ack
```

### 调试技巧

#### 1. 串口日志监控
```bash
idf.py monitor
```

#### 2. 网络连接检查
- 检查设备WiFi连接状态
- 验证MQTT服务器可达性
- 确认防火墙设置

#### 3. 硬件调试
- 检查LED灯带电源
- 验证GPIO连接
- 确认TFT显示屏连接

### 常见问题解决

#### 连接问题
1. **设备无法连接WiFi**
   - 检查SSID和密码配置
   - 验证网络信号强度
   - 检查路由器设置

2. **MQTT连接失败**
   - 验证服务器地址和端口
   - 检查网络防火墙
   - 确认MQTT服务运行状态

#### 功能问题
1. **LED不响应**
   - 检查GPIO配置
   - 验证LED灯带电源
   - 检查硬件连接

2. **TFT显示异常**
   - 检查显示屏连接
   - 验证SPI配置
   - 检查电源供应

## 开发注意事项

### 代码规范
- 使用ESP-IDF编码规范
- 添加必要的错误处理
- 使用FreeRTOS任务安全函数

### 内存管理
- 注意堆栈大小设置
- 合理使用动态内存
- 避免内存泄漏

### 性能优化
- 优化LED刷新频率
- 减少不必要的屏幕刷新
- 合理使用定时器

## 扩展开发

### 添加新效果
1. 在`led_effects.c`中添加效果实现
2. 在`led_effects.h`中声明API
3. 在MQTT处理函数中添加支持

### 硬件扩展
1. 支持更多LED类型
2. 添加传感器集成
3. 扩展显示功能

### 协议扩展
1. 支持更多控制命令
2. 添加设备发现功能
3. 实现OTA升级

## 版本历史

### v1.0.0 (当前版本)
- 基础MQTT控制功能
- 支持74颗WS2812 LED
- TFT状态显示
- 多种LED效果支持

## 技术支持

### 文档资源
- [ESP-IDF官方文档](https://docs.espressif.com/projects/esp-idf/)
- [LED Strip组件文档](https://espressif.github.io/idf-extra-components/latest/led_strip/index.html)

### 社区支持
- [ESP32官方论坛](https://esp32.com/)
- [GitHub Issues](https://github.com/espressif/esp-idf/issues)

---

*本文档最后更新：2024年*