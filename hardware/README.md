# 硬件工程说明

本目录存放硬件设计源文件。目前能找到的是**分控板**的 Altium Designer 工程；主控板的工程文件已丢失，仓库中只保留原理图截图与实物照片，见 [02 硬件设计](../docs/02-hardware.md)。

## 1. 分控板工程

| 文件 | 说明 |
| --- | --- |
| `pcb/light.PrjPcb` | Altium 工程文件，双击打开 |
| `pcb/Sheet1.SchDoc` | 原理图：ESP8266、AMS1117-3.3、光敏电阻接口、舵机接口 |
| `pcb/PCB1.PcbDoc` | PCB 布局，尺寸约 3 × 4 cm |
| `pcb/Schlib1.SchLib` | 原理图库 |
| `pcb/PcbLib1.PcbLib` | PCB 封装库 |
| `pcb/light.PrjPcbStructure` | 工程结构记录 |

工程名为 `light`（灯控），对应论文中的分控端。打开后可直接查看原理图与 PCB；如需打样，导出 Gerber 即可。

## 2. 设计要点

- **供电**：USB 5V 输入，AMS1117-3.3 降压，两路输出分别给 ESP8266 与 SG90 舵机；
- **舵机接口**：3 针排针（信号、5V、GND），信号接 ESP8266 GPIO14；
- **光敏接口**：分压后接 ADC，用于亮度判断；
- **ESP8266**：使用模组而非裸芯片，方便更换与烧录；
- **体积**：板子做小以贴合墙面开关，配合立式 USB 转接板取电。

## 3. 主控板

主控板集成了锂电池充放电、AMS1117-5V 降压、ESP8266 固定座、传感器与语音模块接口。原理图与 PCB 源文件未能找到，文档中的电路图来自毕业设计正文截图：

- [锂电池充放电电路](../images/schematic-battery-charger.png)
- [5V 降压电路](../images/schematic-5v-regulator.png)
- [外设接口电路](../images/schematic-io-connector.png)
- [ESP8266 电路](../images/schematic-esp8266.png)
- [ESP8266 烧录电路](../images/schematic-esp8266-flash.png)
- [主控板实物](../images/main-pcb.jpg)
