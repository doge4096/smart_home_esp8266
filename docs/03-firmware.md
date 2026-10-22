# 03 固件设计

> 本仓库未包含固件源码。本文档根据毕业设计说明书中的程序流程图与关键代码截图，还原主控端与分控端的固件结构、通信协议和实现要点。文中的伪代码用于说明逻辑，不是原始工程代码。

## 1. 主控端固件

主控端基于 STM32C8T6，裸机运行，使用串口中断处理外部事件，没有引入 RTOS。

### 1.1 启动流程

![主控端程序流程](../images/flowchart-main-controller.png)

上电后的顺序：

1. **网络初始化**：通过串口 1 向 ESP8266 发送 AT 指令，配置 WiFi 与 MQTT 参数，直到确认连上互联网和阿里云；
2. **模块初始化**：初始化 DHT20（I2C）、MQ-4（ADC）、红外发射（GPIO）、语音模块串口；
3. **进入主循环**：周期性采集并上报，同时等待串口中断。

### 1.2 主循环与定时上报

主循环以 5 秒为周期轮询，维护一个计数器；计数达到 60 时采集并上报一次环境数据，然后清零。这样做的目的是降低上报频率、减少流量与功耗。

```c
// 逻辑示意
uint16_t count = 0;

while (1) {
    if (count >= 60) {
        sensors_read();        // 读 DHT20 / MQ-4
        env_data_publish();    // 通过 ESP8266 发布 MQTT
        count = 0;
    }
    delay_ms(5000);
    count++;
}
```

除了定时上报，主控端还支持“立即上报”：串口 1 收到字符串 `get` 时，马上采集并发送一次数据，同时清空计数。

```c
// 串口 1 中断（与 ESP8266 通信），逻辑示意
void USART1_IRQHandler(void) {
    if (uart1_received("get")) {
        sensors_read();
        env_data_publish();
        count = 0;
    }
}
```

### 1.3 语音控制

语音模块 HLK-V20 接在串口 2 上，识别到关键词后直接输出结果字符串。STM32 在串口 2 中断中解析结果，再通过串口 1 向 ESP8266 发送 AT 指令，发布对应的控制消息。

```c
// 串口 2 中断（与语音模块通信），逻辑示意
void USART2_IRQHandler(void) {
    char *cmd = uart2_read_line();
    if (cmd == NULL) return;

    if (is_light_on(cmd))       mqtt_publish("light_on");
    else if (is_light_off(cmd)) mqtt_publish("light_off");
    else if (is_ac_on(cmd))     ir_send_ac_on();
    else if (is_ac_off(cmd))    ir_send_ac_off();
}
```

语音模块的关键词与输出内容通过模块厂商的配置网页设置，再由厂商生成固件烧录，不需要在主控里做语音算法。

### 1.4 环境采集

**DHT20（I2C）**

DHT20 是 I2C 接口的数字温湿度传感器。STM32 向模块发送请求码，等待转换完成后读取温湿度原始值，再按数据手册换算为实际值。

**MQ-4（ADC）**

MQ-4 输出模拟电压，STM32 将引脚配置为 ADC 模式，读取电压后按传感器特性曲线换算为气体浓度。项目中只做相对浓度判断，用于超阈值告警，不追求绝对标定。

**光敏电阻（分控端 ADC）**

光敏电阻与固定电阻分压后接入 ESP8266 的 ADC 引脚，固件直接调用 SDK 的 ADC 读取函数获取电压，再换算为亮度百分比。

### 1.5 红外空调控制

红外发射模块由 GPIO 驱动，固件按空调遥控器的编码协议发送对应码字。语音或 APP 下发“开空调/关空调”时触发。

## 2. 分控端固件

分控端以 ESP8266 为主控，基于 ESP8266 SDK 开发，负责三件事：采集亮度、驱动舵机、收发 MQTT。

### 2.1 启动流程

![分控端程序流程](../images/flowchart-sub-controller.png)

1. 连接 WiFi；
2. 初始化 MQTT 并连接阿里云；
3. 初始化 ADC 与 PWM；
4. 进入循环，等待 MQTT 消息或定时事件。

### 2.2 舵机 PWM

SG90 通过 PWM 控制角度。ESP8266 SDK 的 PWM 初始化需要提供一组 IO 信息三元组（端口、功能、IO 编号），保存在 `n × 3` 的数组中，再设置 PWM 周期与分频系数，最后调用 `pwm_start` 启动。

```c
// 逻辑示意
uint32 io_info[][3] = {
    {PWM_0, FUNC_GPIO12, 12},   // 舵机信号脚
};
pwm_init(20000, duty, 1, io_info);  // 20ms 周期
pwm_start();
```

开关灯时改变占空比，让舵机转到对应角度，带动拨片按压开关，保持一段时间后复位。

### 2.3 命令解析

分控端订阅阿里云的广播 topic。收到消息后，把数据部分存入缓存变量，与预设字符串比对，决定执行开灯、关灯还是其他操作。

```c
// 逻辑示意
void mqtt_callback(char *topic, char *data) {
    if (strcmp(data, "light_on") == 0) {
        servo_set(OPEN_ANGLE);
    } else if (strcmp(data, "light_off") == 0) {
        servo_set(CLOSE_ANGLE);
    } else if (strcmp(data, "get_light") == 0) {
        light_publish(adc_read_light());
    }
}
```

亮度数据不做定时上报，只在收到上报命令时发送一次，减少无效流量。

### 2.4 自动关灯

分控端联网后获取当前时间。每天第一次到达设定时间（如上午 10 点）时，读取一次环境亮度：

- 亮度高于阈值 → 执行关灯；
- 亮度低于阈值 → 不动作。

阈值由 APP 下发并保存在分控端，用户可以根据房间朝向和季节调整。

```c
// 逻辑示意
if (now.hour == 10 && !done_today) {
    if (adc_read_light() > light_threshold) {
        servo_set(CLOSE_ANGLE);
    }
    done_today = true;
}
```

## 3. 通信协议

### 3.1 物模型

阿里云产品 `smart_home` 定义了 9 个属性，覆盖主控端与两个分控端：

| 功能名称 | 标识符 | 类型 | 取值范围 |
| --- | --- | --- | --- |
| 控制命令 | `control` | int32 | 0–100 |
| 卧室温度 | `temp_bedroom` | float | -50–50 |
| 卧室湿度 | `hum_bedroom` | float | 0–100 |
| 卧室可燃气体 | `bedroom` | float | 0–100 |
| 卧室亮度 | `light_bedroom` | float | 0–100 |
| 客厅温度 | `temp_parlor` | float | -50–50 |
| 客厅湿度 | `hum_parlor` | float | 0–100 |
| 客厅可燃气体 | `parlor` | float | 0–100 |
| 客厅亮度 | `light_parlor` | float | 0–100 |

`control` 是整型命令字，具体取值由固件与 APP 约定，用于开灯、关灯、开空调、关空调等动作。

### 3.2 Topic 结构

阿里云 IoT 的系统 topic 格式为：

```
/{ProductKey}/{DeviceName}/user/change
```

本项目在云端创建设备后，规则引擎把消息转发到以下 topic：

| 目标 | Topic |
| --- | --- |
| 卧室分控 | `/a1R7nAf2L2O/switch_bedroom/user/change` |
| 客厅分控 | `/a1R7nAf2L2O/switch_parlor/user/change` |
| 卧室主控 | `/a1R7nAf2L2O/control_bedroom/user/change` |
| 客厅主控 | `/a1R7nAf2L2O/control_parlor/user/change` |
| 手机 APP | `/a1R7nAf2L2O/APP_control/user/change` |

> 截图中出现的 ProductKey 属于已停用的测试实例；实际部署时应替换为自己的 ProductKey 与 DeviceSecret，不要把密钥提交到仓库。

### 3.3 消息格式

设备与云端之间使用 JSON 收发。上报环境数据时，把温度、湿度、气体浓度、亮度按物模型标识符组织；下发控制命令时，使用 `control` 字段或约定的字符串命令。

### 3.4 消息广播

云端规则引擎的查询语句为：

```sql
SELECT * FROM "/a1R7nAf2L2O/+/user/change"
```

即订阅产品下所有设备的 `user/change` 消息，再分别转发到各分控、主控和 APP 的 topic。这样任意设备发出的命令，相关设备都能收到，不需要在设备侧维护地址表。

## 4. 固件设计中的取舍

| 问题 | 选择 | 原因 |
| --- | --- | --- |
| 主控是否上 RTOS | 裸机 + 中断 | 任务简单，裸机足够，减少资源占用 |
| 环境数据上报频率 | 5 秒轮询、60 次触发 | 降低流量与功耗，同时保留 `get` 立即上报 |
| 分控亮度是否定时上报 | 否，按需上报 | 亮度变化慢，定时上报没有意义 |
| 命令用数字还是字符串 | 字符串比对 | 可读性好，调试方便，设备数量少 |
| 阈值存哪里 | 分控端本地 | 断网时仍能按本地阈值判断，但时间需联网校准 |

## 5. 本章小结

主控端以 STM32 处理语音、传感器与红外，ESP8266 只负责联网；分控端用 ESP8266 同时承担控制与通信。两端通过阿里云 MQTT 交换数据，规则引擎负责广播。固件结构简单，重点在于把“采集—上报—下发—执行”这条链路做稳定。
