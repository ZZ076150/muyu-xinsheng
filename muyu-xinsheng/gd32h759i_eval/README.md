# GD32H759I-EVAL 主控固件（云台追踪 + 安全监测）

本目录为主控固件说明：基于 **GD32H759I-EVAL + RT-Thread** 的多线程实现，负责云台人脸追踪、药盒/设备翻倒检测、GPS 定位上报、串口屏交互、MP3 播报，以及与 K230、ESP8266 的串口通信。

| 项目 | 说明 |
| --- | --- |
| 主控芯片 | GD32H759IMK6（ARM Cortex-M7，主频最高 600MHz） |
| 开发板 | GD32H759I-EVAL |
| 开发环境 | RT-Thread 官方 BSP `bsp/gd32/arm/gd32h759i-eval`（Keil MDK / IAR） |
| 参考手册 | GD32H759xx Datasheet、GD32H737/757/759 User Manual（gd32mcu.com「资料下载 → H7」） |

> K230 与 ESP8266 端代码无需修改，串口字节协议保持一致，接线按本文引脚表接到 GD32 即可。

---

## 1. 串口分配（逻辑端口 ↔ RT-Thread 设备 ↔ 外设）

| 逻辑端口 | RT-Thread 设备 | 外设 | 波特率 | 用途 | RT-Thread BSP 默认引脚 | 复用功能 |
| --- | --- | --- | --- | --- | --- | --- |
| 控制台 | `uart0` | USART0 | 115200 | 调试日志 rt_kprintf | TX=PA9 / RX=PA10 | AF7 |
| K230 | `uart1` | USART1 | 115200 | 人脸偏移 `FACE,...` | TX=PA2 / RX=PA3 | AF7 |
| ESP8266 | `uart2` | USART2 | 115200 | start/stop、medicine、摔倒、GPS JSON、屏命令转发 | TX=PB10 / RX=PB11 | AF7 |
| 串口屏 | `uart3` | UART3 | 9600 | AA/55 命令帧 | TX=PC10 / RX=PC11 | AF8 |
| GPS | `uart4` | UART4 | 9600 | NMEA `$GNGGA` | TX=PB6 / RX=PB5 | AF14 |
| MP3 | `uart6` | UART6 | 9600 | DY-SV17F 8 字节帧（只用 TX） | TX=PE8 / RX=PE7 | AF7 |

### 引脚表注意事项（务必先看）

1. **GD32H7 没有 USART3/USART4**！H7 系列编号是 USART0/1/2/5 + UART3/4/6/7，
   表中 `uart3→UART3、uart4→UART4、uart6→UART6` 是 RT-Thread 的设备名编号，
   不是 STM32 习惯的串口号。
2. `uart0~uart3` 的引脚是 RT-Thread 官方 BSP 的 Kconfig 默认值；
   `uart4 / uart6` 的引脚 + AF 为按数据手册复用功能表核实的建议值
   （UART4=AF14、UART6=AF7），**请对照 EVAL 板原理图/扩展排针再确认一次**：
   - 若 PB5/PB6 被板载资源占用：UART4 可换 TX=PB13 / RX=PB12（同样 AF14）
   - 若 PE7/PE8 被 SDRAM（EXMC 数据线）占用：UART6 可换其他 AF7 组合，
     例如 TX=PB4 / RX=PB3（注意 PB3/PB4 是 JTDO/NJTRST 调试口，需保留 SWD 时慎用）
3. 引脚与 AF 都是 `menuconfig` 里的字符串，改引脚不需要动代码。

---

## 2. 普通 GPIO 分配（步进电机 / 红外）

| 信号 | GD32 默认引脚 | 电平/说明 |
| --- | --- | --- |
| 步进 STEP | **PB0**（`STEP_GPIO_*`） | 由 TIMER5 中断翻转 |
| 步进 DIR | **PB1**（`DIR_GPIO_*`） | 方向 |
| 步进 ENA | **PC3**（`ENA_GPIO_*`） | 高电平使能 |
| 红外对射 | **PC2**（`IR_GPIO_*`） | 上拉输入，无遮挡=低电平 → 发 medicine |

这些都在 `bsp/bsp_config.h` 顶部集中定义，改板子只改这一个文件。

> ⚠️ PB2 是 BOOT1 的引脚，已刻意避开（避免影响启动模式）。

---

## 3. 需要补的 BSP 配置（menuconfig + board/Kconfig）

RT-Thread `gd32h759i-eval` 的 `board/Kconfig` 默认只提供 UART0~UART3 菜单项，
`uart4`、`uart6` 需要手工把下面两段追加到
`bsp/gd32/arm/gd32h759i-eval/board/Kconfig` 的 `BSP_USING_UART3` 后面
（照抄 UART3 结构即可，引脚/AF 值即第 1 节表格）：

```kconfig
            menuconfig BSP_USING_UART4
                bool "Enable UART4"
                default n
                if BSP_USING_UART4
                    config BSP_UART4_TX_PIN
                        string "UART4 TX name, such as PB6"
                        default "PB6"

                    config BSP_UART4_RX_PIN
                        string "UART4 RX name, such as PB5"
                        default "PB5"

                    config BSP_UART4_AFIO
                        string "UART4 alternate function, such as AF14"
                        default "AF14"
                endif

            menuconfig BSP_USING_UART6
                bool "Enable UART6"
                default n
                if BSP_USING_UART6
                    config BSP_UART6_TX_PIN
                        string "UART6 TX name, such as PE8"
                        default "PE8"

                    config BSP_UART6_RX_PIN
                        string "UART6 RX name, such as PE7"
                        default "PE7"

                    config BSP_UART6_AFIO
                        string "UART6 alternate function, such as AF7"
                        default "AF7"
                endif
```

然后在工程目录执行 `menuconfig`：

- `Hardware Drivers Config → Enable UART → UART1/2/3/4/6` 全部勾选
  （UART0 是控制台，保持默认）
- 串口框架保持 **Serial V1**（`Select UART framework version → Use Serial V1 framework`），
  本工程 BSP 代码按 V1 的 `RT_DEVICE_CTRL_CONFIG + INT_RX` 写法实现
- 摔倒检测：`Hardware Drivers Config → Enable I2C → Enable Hardware I2C → Enable I2C0 Hardware BUS`
  （驱动内置引脚：hwi2c0 = SCL PB8 / SDA PB9，AF4；MPU6050 接这两个脚）

保存后 `pkgs --update`，再 `scons --target=mdk5` 重新生成 Keil 工程。

---

## 4. 编译步骤（Keil MDK）

1. 克隆 RT-Thread（5.0.0 或更高），进入 `bsp/gd32/arm/gd32h759i-eval`；
2. 按第 3 节完成 menuconfig 配置；
3. 把本目录的 `main.c`、`applications/*.c`、`bsp/*.c` 加入工程
   （替换 BSP 自带的 `applications/main.c`），
   `applications/`、`bsp/` 加入头文件搜索路径；
4. 编译前确认 `bsp_config.h` 中：串口设备名、`STEPPER_TIMER_CLK_HZ`
   （默认按 CK_TIMERS=300MHz 计算，实际以 `system_gd32h7xx.c` 的时钟树为准）、
   GPIO 宏与 EVAL 板一致；
5. 下载运行，接 uart0（PA9/PA10）看 115200 启动日志：

```text
========================================
  舒衡智养 · 适老云台追踪  GD32H759I-EVAL
  RT-Thread 5.x.x
========================================
[UART] uart1 就绪 baud=115200
...
[MAIN] 系统启动完成，云台等待 K230 人脸数据
```

---

## 5. 线程划分

| 线程 | 优先级 | 栈 | 周期 | 职责 |
| --- | --- | --- | --- | --- |
| motion | 8 | 2048 | 20ms | 云台状态机：偏移换算脉冲、2s 采样、10s 自动搜索 |
| vision | 10 | 1024 | 事件 | uart1 收 K230 `FACE,...`，解析 offset_x |
| net | 10 | 1024 | 事件 | uart2 收 ESP8266 `start`/`stop` |
| fall | 11 | 1024 | 100ms | MPU6050 倾角 >45° 持续 1s → 发 `DEVICE_FALL_DETECTED` |
| screen | 12 | 1024 | 20ms | uart3 收 AA/55 帧：a~e 本地播放/开搜索，f/g~r/1~6 转发 ESP |
| gps | 12 | 1024 | 事件 | uart4 收 NMEA → 北京时间 → JSON 发 ESP |
| ir | 13 | 512 | 10ms | 红外 20ms 防抖，无遮挡 → 发 `medicine` |

---

## 6. 报文清单

| 链路 | 方向 | 报文 |
| --- | --- | --- |
| K230 → 主控 | uart1 | `FACE,<cx>,<cy>,<w>,<h>,<offset_x>,<offset_y>\r\n`（主控只用 offset_x） |
| 主控 → ESP8266 | uart2 | `medicine` / `DEVICE_FALL_DETECTED` / GPS JSON / 屏命令转发（`f`,`g`~`r`,`1`~`6`） |
| ESP8266 → 主控 | uart2 | `start` / `stop`（开关云台自动搜索） |
| 串口屏 → 主控 | uart3 | `0xAA | cmd | 0x55`，cmd 为 'a'~'e'（本地）或 'f'~'r'、'1'~'6'（转发） |
| GPS → 主控 | uart4 | NMEA `$GNGGA` 行 |
| 主控 → MP3 | uart6 | `7E FF 06 03 00 HH LL EF`（播放 HHLL 号曲目） |

---

## 7. 注意事项

- 源文件均为 **UTF-8 编码**。Keil MDK 若提示乱码，在
  `Edit → Configuration → Encoding` 里选 UTF-8 再重新打开；用 GB2312 打开会乱码。
- 串口屏协议保持 `AA cmd 55`；若屏幕实际发的是文本命令（TJC 的 `xxx.txt=...`），
  请改 `bsp/bsp_screen.c` 的 `screen_feed()` 状态机。
- `bsp_uart_init()` 会强制要求 uart1/2/3/4/6 五个设备都存在；如果暂时不接某个模块，
  把 `bsp/bsp_uart.c` 里 `g_uart[]` 对应行注释掉即可。
- RT-Thread 的 tick 必须为 1000（`RT_TICK_PER_SECOND = 1000`，BSP 默认值），
  本工程所有 `rt_tick_get_millisecond()` 计时都依赖该配置。
- MP3 模块是 9600 波特率（DY-SV17F 出厂默认）。
- 摔倒检测的 I2C 用 `hwi2c0`（PB8/PB9），BSP 的 `drv_hard_i2c.c` 内置了引脚表；
  想换总线/引脚改驱动表，不要在应用层改。
- 上电默认 `enableAutoSearch = true`；串口屏 `e` 重新打开搜索，
  ESP8266 的 `start/stop` 可远程开关。
