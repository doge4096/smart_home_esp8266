# 固件说明

本目录是智能家居系统的主控端与分控端固件源码，均来自毕业设计时的实际工程。为便于公开，已做两处处理：

1. **敏感信息脱敏**：WiFi 账号密码、阿里云 ProductKey、DeviceSecret 全部替换为 `YOUR_*` 占位符；
2. **编码转换**：原工程为 GBK 编码，已统一转为 UTF-8，中文注释在 GitHub 上可正常显示。

## 1. 目录对应关系

| 目录 | 对应设备 | 说明 |
| --- | --- | --- |
| `stm32/control_bedroom` | 卧室主控 | STM32F103C8T6，采集温湿度/气体、语音解析、红外 |
| `stm32/control_parlor` | 客厅主控 | 同上，MQTT topic 与设备名不同 |
| `esp8266/switch_bedroom` | 卧室分控 | ESP8266，光敏采集 + 舵机拨开关 |
| `esp8266/switch_parlor` | 客厅分控 | 同上 |

两个主控、两个分控的代码结构完全相同，区别只在设备名、ProductKey、MQTT topic 与发布字段（`bedroom_*` / `parlor_*`）。新增房间时复制一份目录，改这几处即可。

## 2. 编译说明

### 2.1 主控端（STM32）

- **IDE**：Keil MDK 5
- **器件**：STM32F103C8（Cortex-M3）
- **依赖**：STM32F10x 标准外设库（已随工程放在 `STM32F10x_FWLib/`）

打开对应目录下的 `USER/PWM.uvprojx`，编译后通过 ST-Link 烧录。工程使用 STM32F10x 标准库，没有使用 HAL/CubeMX。

目录说明：

| 目录 | 内容 |
| --- | --- |
| `CORE` | 启动文件与 CMSIS 内核文件 |
| `STM32F10x_FWLib` | 标准外设库 |
| `HARDWARE` | ADC、I2C、AHT20、红外、LED、按键、定时器驱动 |
| `SYSTEM` | 延时、串口 1（ESP8266）、串口 2（语音模块） |
| `USER` | `main.c` 与 Keil 工程文件 |

### 2.2 分控端（ESP8266）

- **SDK**：ESP8266 NONOS SDK（非 RTOS）
- **工具链**：官方 `xtensa-lx106-elf` 工具链 + `make`
- **MQTT**：工程内自带 Tuan PM 的 MQTT 客户端（`mqtt/`）

编译前需要设置 `SDK_PATH` 环境变量指向 NONOS SDK 根目录，然后在工程目录执行 `make`。也可以用 `gen_misc.bat`（Windows）生成固件。烧录使用串口工具，把 GPIO0 拉低后复位进入下载模式。

> 工程内的 `include/`、`modules/`、`mqtt/` 是当年从 SDK 示例中复制出来的必要文件，不含完整 SDK。编译前请确认 SDK 版本与 `Makefile` 中的路径一致。

## 3. 配置占位符

源码中所有需要按环境替换的值：

| 占位符 | 文件 | 说明 |
| --- | --- | --- |
| `YOUR_WIFI_SSID` | `esp8266/*/include/mqtt_config.h`、`stm32/*/SYSTEM/usart2/usart2.c`（注释） | WiFi 名称 |
| `YOUR_WIFI_PASSWORD` | 同上 | WiFi 密码 |
| `YOUR_PRODUCT_KEY` | 四个工程均有 | 阿里云产品 ProductKey |
| `YOUR_MQTT_PASSWORD` | `mqtt_config.h`、`usart2.c` | 阿里云设备密钥（HMAC 签名） |

主控端的 MQTT 用户名格式为 `DeviceName&ProductKey`，例如 `control_bedroom&YOUR_PRODUCT_KEY`。分控端在 `mqtt_config.h` 中分别配置 `MQTT_USER`、`MQTT_PASS`、`STA_SSID`、`STA_PASS`。

替换后搜索 `YOUR_` 确认没有遗漏。

## 4. 关键实现

| 功能 | 位置 |
| --- | --- |
| 主控主循环、定时上报 | `stm32/*/USER/main.c` |
| AT 指令联网与 MQTT 发布 | `stm32/*/SYSTEM/usart2/usart2.c` |
| 语音命令字节映射 | `stm32/*/SYSTEM/usart2/usart2.c` 的 `USART2_IRQHandler` |
| AHT20 驱动 | `stm32/*/HARDWARE/AHT20-21_DEMO_V1_3/` |
| 红外发射 | `stm32/*/HARDWARE/REMOTE/` |
| 分控初始化、WiFi、SNTP、MQTT 回调 | `esp8266/*/user/user_main.c` |
| 分控配置 | `esp8266/*/include/mqtt_config.h` |

更详细的流程说明见 [03 固件设计](../docs/03-firmware.md)。

## 5. 已知问题

- 主控端 `esp_init()` 用固定延时等待 AT 指令返回，没有判断返回值，启动时间较长且失败后不会重试；
- 分控端自动关灯的判断写在 MQTT 回调里，依赖收到消息才执行，属于实现缺陷；
- 消息体没有统一 schema，靠字符串逐条比对。

这些问题在 [06 复盘](../docs/06-retrospective.md) 中有分析。
