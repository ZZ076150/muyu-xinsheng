# 目语心声 · 阿尔茨海默症老人智能辅具

> 用眼睛说话，让爱被听见。

一套面向阿尔茨海默症（AD）老人居家照护场景的一体化智能辅具开源实现，由**下位机智能辅具设备**与**上位机多端应用**组成。核心交互为「目语心声」模式：屏幕以左右双图呈现选项，老人**注视目标图片**即可完成需求表达，把"说不出口的话"翻译成家属能读懂的信号。

本仓库为下位机（嵌入式）部分：

- **K230** 视觉识别（人脸、摔倒、人形、人眼朝向）与视频流上传
- **兆易创新 GD32H759I-EVAL** 主控（云台人脸跟踪、药盒检测、设备翻倒检测、多模块串口调度），基于 **RT-Thread** 多线程实现
- **ESP8266** MQTT 网关（数据上报与控制指令转发）
- **天问 ASRPRO** 语音播报

> 主控已完成从 Arduino Mega 2560 到 **GD32H759I-EVAL（ARM Cortex-M7，主频最高 600MHz）** 的更换，固件基于 RT-Thread 设备框架开发，见 [`gd32h759i_eval/`](gd32h759i_eval/README.md)。

---

## 一、功能特性

### 1. 目语心声交互（核心）
- 屏幕提供「情绪 / 需求 / 行为」三类选项，每类以左右两张图片呈现
- 双通道输入：**人眼朝向识别**（注视左/右图即选择）＋ **触屏**兜底
- 选择结果由语音模块实时播报，实现"眼睛即语言"

### 2. AI 视觉识别（K230）
- 人脸识别：辨认老人与家属身份，支持到访提醒
- 摔倒检测：识别跌倒等异常姿态并触发告警
- 人形检测：判断老人是否离开画面，实现防走失提醒
- 人眼朝向识别：支撑目语心声交互
- 人脸自动搜索与锁定跟踪：云台持续对准目标人脸

### 3. 安全监护
- 设备翻倒检测（MPU6050）
- GPS 定位与位置上报（ATGM3365，NMEA $GNGGA）
- 智能起夜灯：光敏传感器判断天黑 ＋ 视觉识别到老人，双条件自动点亮（直连 K230）

### 4. 生活辅助与情感陪伴
- 智能感应药盒：红外对射模块感应药盒开合状态
- 音频播放：DY-SV17F MP3 模块，屏幕点歌（8 字节帧指令）
- 语音播报：天问 ASRPRO 2.0（由 ESP8266 软串口下发）
- AI 语音陪伴：ESP32-S3「小智」智能体（独立子系统，麦克风 INMP441 + 功放 MAX98357）

### 5. 远程守护
- K230 经板载 WiFi 以 **TCP 直传**实时画面至后端
- ESP8266 以 **MQTT** 上报摔倒、人形、目语心声选择、设备翻倒、GPS、药盒等状态
- 上位机（UniApp 三端：小程序 / 网页 / App）展示数据并推送告警

---

## 二、硬件组成

| 模块 | 选型 | 接口 | 说明 |
|---|---|---|---|
| 主控 | **兆易创新 GD32H759I-EVAL**（GD32H759，ARM Cortex-M7，主频最高 600MHz） | 多路 USART / I2C / GPIO / TIMER | RT-Thread 多线程调度、云台人脸跟踪、屏交互、传感器上报 |
| 视觉 | 幻尔 K230 视觉开发板（CanMV / MicroPython） | UART ＋ 板载 WiFi | 全部视觉识别与视频流上传 |
| 人机交互屏 | 淘晶驰（TJC）串口屏 | UART（9600） | 目语心声双图与触屏输入，AA/55 协议帧 |
| 云台 | 42 步进电机 ＋ 驱动器（32 细分，一圈 6400 脉冲） | 脉冲 / 方向 / 使能 | 一维云台人脸跟踪，TIMER 中断发脉冲 |
| 姿态 | MPU6050 | I2C（hwi2c0，400kHz） | 设备翻倒检测 |
| 定位 | ATGM3365 GPS | UART（9600） | 定位上报 |
| 光线 / 照明 | 光敏传感器 ＋ 起夜灯 LED | 直连 K230（ADC / GPIO） | 天黑且识别到老人时点灯 |
| 药盒 | 红外对射模块 | GPIO | 开合状态感应 |
| 联网 | ESP8266 | UART（115200）＋ WiFi | MQTT 消息上报与控制指令转发 |
| 音频 | DY-SV17F MP3 模块 | UART（9600） | 屏幕点歌与提示音 |
| 语音陪伴 | ESP32-S3（小智固件）＋ INMP441 ＋ MAX98357 | I2S / WiFi | AI 语音对话（独立运行） |

系统硬件框架图见 [`docs/系统硬件框架图.png`](docs/系统硬件框架图.png)。

---

## 三、目录结构

```
.
├─ gd32h759i_eval/                          # 主控固件（GD32H759I-EVAL + RT-Thread）
│   ├─ main.c                               # 线程创建 + 初始化
│   ├─ README.md                            # 串口分配、引脚映射、Kconfig 扩展、编译步骤
│   ├─ applications/                        # RT-Thread 线程
│   │   ├─ task_pantilt.c/.h                # 云台状态机（偏移追踪 / 自动搜索）
│   │   ├─ thread_vision.c                  # K230 FACE 报文解析（uart1）
│   │   ├─ thread_screen.c                  # 串口屏 AA/55 指令处理（uart3）
│   │   ├─ thread_net.c                     # ESP8266 start/stop（uart2）
│   │   ├─ thread_fall.c                    # MPU6050 摔倒检测
│   │   ├─ thread_gps.c                     # GPS NMEA → JSON（uart4）
│   │   └─ thread_ir.c                      # 红外对射防抖（GPIO）
│   └─ bsp/
│       ├─ bsp_config.h                     # 串口设备名 / 波特率 / GPIO 宏集中配置
│       ├─ bsp_uart.c/.h                    # 5 路硬件串口 + 行缓存
│       ├─ bsp_stepper.c/.h                 # TIMER 中断步进脉冲（加速曲线）
│       ├─ bsp_mpu6050.c/.h                 # MPU6050 寄存器驱动（I2C 设备框架）
│       ├─ bsp_screen.c/.h                  # AA/55 逐字节帧状态机
│       ├─ bsp_gps.c/.h                     # GNGGA 解析 + JSON 定点格式化
│       ├─ bsp_ir.c/.h                      # 红外对射输入
│       └─ bsp_mp3.c/.h                     # DY-SV17F 8 字节帧
├─ k230/
│   └─ main_k230.py                         # K230 主程序（视觉识别 + 目语心声 + 视频流）
├─ esp8266/
│   └─ esp8266_mqtt_gateway/
│       └─ esp8266_mqtt_gateway.ino         # MQTT 网关与串口转发
├─ tianwen/
│   └─ 天问.hd                              # 天问Block 语音播报工程
└─ docs/
    └─ 系统硬件框架图.png
```

> ESP8266 目录为 Arduino 工程：**文件夹名必须与 .ino 文件名一致**，用 Arduino IDE 打开对应文件夹即可。

---

## 四、快速开始

### 1. GD32H759I-EVAL 主控（RT-Thread + Keil MDK）

1. 取 RT-Thread 官方 BSP `bsp/gd32/arm/gd32h759i-eval`，按 `gd32h759i_eval/README.md` 第 3 节补齐 UART4 / UART6 的 Kconfig 菜单项；
2. menuconfig 中使能 `UART1/2/3/4/6` 与硬件 I2C0，串口框架选 **Serial V1**；
3. 将本目录 `main.c`、`applications/*.c`、`bsp/*.c` 加入工程（替换 BSP 自带 `applications/main.c`），头文件路径加入 `bsp/`、`applications/`；
4. 编译下载，接 uart0（PA9/PA10，115200）查看启动日志；
5. 串口分配（详见 README）：

| RT-Thread 设备 | 外设 | 波特率 | 连接对象 |
| --- | --- | --- | --- |
| `uart0` | USART0 | 115200 | 调试控制台 |
| `uart1` | USART1 | 115200 | K230（人脸偏移 FACE 报文） |
| `uart2` | USART2 | 115200 | ESP8266（MQTT 网关） |
| `uart3` | UART3 | 9600 | 淘晶驰串口屏（AA/55 帧） |
| `uart4` | UART4 | 9600 | GPS 模块（NMEA） |
| `uart6` | UART6 | 9600 | DY-SV17F MP3（8 字节帧） |

### 2. K230（CanMV IDE）

1. 用 CanMV IDE 打开 `k230/main_k230.py`；
2. 修改文件顶部的 WiFi 与服务器配置（见第五节）；
3. UART1（115200）接 ESP8266（眼动/摔倒/人形/FACE_LOST），UART2（115200）接 GD32 主控（人脸偏移 FACE 报文）；
4. 光敏传感器与起夜灯 LED 按代码中的 GPIO 定义接线。

### 3. ESP8266（Arduino IDE）

1. 安装 ESP8266 开发板支持，以及库：`PubSubClient`、`ArduinoJson`；
2. 修改顶部 WiFi 与 MQTT Broker 配置；
3. 引脚：D2/D1 接 K230，D3/D4 接 GD32 主控，D5/D6 接天问模块（软串口）。

### 4. 天问模块

用天问Block 打开 `tianwen/天问.hd`，烧录后即可接收播报指令。

---

## 五、配置说明（重要）

仓库中所有凭据均已替换为占位符，**请填入你自己的配置**：

| 占位符 | 位置 | 含义 |
|---|---|---|
| `YOUR_WIFI_SSID` | K230 / ESP8266 | WiFi 名称 |
| `YOUR_WIFI_PASSWORD` | K230 / ESP8266 | WiFi 密码 |
| `YOUR_SERVER_IP` | K230 | 视频流接收服务器 IP（默认端口 9002） |
| `YOUR_MQTT_BROKER_IP` | ESP8266 | MQTT Broker 地址（默认端口 1883） |

> ⚠️ **请勿把真实的 WiFi 密码、服务器地址、MQTT 凭据提交到公开仓库。**

---

## 六、通信协议

### 1. 串口链路

| 链路 | 方向 | 帧格式 | 说明 |
| --- | --- | --- | --- |
| K230 → 主控 | uart1 | `FACE,<cx>,<cy>,<w>,<h>,<offset_x>,<offset_y>\r\n` | 人脸框中心、尺寸与水平偏移；主控只用 offset_x 驱动云台 |
| K230 → ESP8266 | UART1 | `left` / `right` / `center`、`fall`、`person`、`FACE_LOST` | 眼动方向与视觉事件，经 MQTT 上报 |
| 主控 → ESP8266 | uart2 | `medicine`、`DEVICE_FALL_DETECTED`、GPS JSON、屏命令转发（`f`、`g`~`r`、`1`~`6`） | 药盒无遮挡 / 倾倒 / 定位 / 呼叫与选择命令 |
| ESP8266 → 主控 | uart2 | `start` / `stop` | 云台自动搜索总开关 |
| 串口屏 → 主控 | uart3 | `0xAA | cmd | 0x55` | cmd 为 `a`~`e`（本地：点歌/开搜索）或 `f`~`r`、`1`~`6`（转发） |
| GPS → 主控 | uart4 | NMEA `$GNGGA` | 经纬度、时间（UTC+8 转换） |
| 主控 → MP3 | uart6 | `7E FF 06 03 00 HH LL EF` | 播放 HHLL 号曲目 |
| ESP8266 → 天问 | 软串口 D5/D6 | 单字母 | 语音播报指令 |

### 2. MQTT 主题

| 主题 | 方向 | 说明 |
|---|---|---|
| `device/elder_2/fall` | 设备 → 后端 | 老人摔倒告警 |
| `device/elder_2/presence` | 设备 → 后端 | 人形检测 / 防走失 |
| `device/elder_2/gaze_cmd` | 设备 → 后端 | 目语心声选择结果 |
| `device/elder_2/mpu6050_flag` | 设备 → 后端 | 设备翻倒状态 |
| `device/elder_2/gps` | 设备 → 后端 | 经纬度等定位信息（JSON） |
| `device/elder_2/medicine_box` | 设备 → 后端 | 药盒开合状态 |
| `device/elder_2/call` | 设备 → 后端 | 一键呼叫 |
| `device/elder_2/camera_follow/control` | 后端 → 设备 | 云台自动搜索开关（`start` / `stop`） |

---

## 七、主控线程划分（RT-Thread）

| 线程 | 优先级 | 栈 | 周期 | 职责 |
| --- | --- | --- | --- | --- |
| motion | 8 | 2048 | 20ms | 云台状态机：偏移换算脉冲、2s 采样、10s 自动搜索 |
| vision | 10 | 1024 | 事件 | uart1 收 K230 FACE 报文 |
| net | 10 | 1024 | 事件 | uart2 收 ESP8266 start/stop |
| fall | 11 | 1024 | 100ms | MPU6050 倾角 >45° 持续 1s 告警 |
| screen | 12 | 1024 | 20ms | uart3 处理 AA/55 帧，本地执行 / 转发 ESP |
| gps | 12 | 1024 | 事件 | uart4 NMEA 解析 → JSON 发 ESP |
| ir | 13 | 512 | 10ms | 红外 20ms 防抖，无遮挡上报 medicine |

---

## 八、安全与隐私提示

- ⚠️ 本项目为**教学与科研原型**，非医疗器械，不能作为医疗诊断依据，也不能替代人工照护。
- ⚠️ 涉及老人影像、人脸与健康数据时，请遵守相关法律法规，取得家属授权，并优先在本地处理、加密传输。
- ⚠️ 老年照护场景下请控制设备动作幅度与运行速度，避免碰撞与惊吓。

---

## 九、开源许可

本项目基于 [MIT License](LICENSE) 开源，欢迎学习与二次开发；如用于论文、竞赛或产品，请注明出处。
