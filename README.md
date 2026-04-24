# Blackbox：基于 STM32 的多传感器事件黑匣子异常记录仪

Blackbox 是一个基于 **STM32F103C8T6** 的嵌入式异常记录项目，用于检测设备的移动、倾斜、冲击等异常事件，并将事件日志保存到外部 **W25Qxx Flash** 中。  
项目支持 **MPU6050 姿态采集、Flash 掉电日志保存、HC-05 蓝牙串口通信、Python 上位机实时监控**，形成了从采集、检测、记录、传输到显示的完整闭环。

> 项目定位：本科阶段嵌入式工程实践项目，重点体现外设驱动、协议设计、数据持久化、上位机联调和调试复盘能力。

---

## 1. 功能特性

- **运动检测**
  - 使用 MPU6050 采集加速度和陀螺仪数据
  - 支持 `NONE / MOVE / TILT / SHOCK` 状态
  - 支持基准校准、连续命中、连续释放、事件优先级和事件升级

- **黑匣子日志**
  - 使用 W25Qxx Flash 保存异常事件
  - 支持掉电保存
  - 支持读取最后一条日志
  - 支持清空日志
  - 修复 W25Qxx 跨页写导致日志损坏的问题
  - 写入后增加回读校验

- **通信与上位机**
  - STM32 通过 USART1 与 HC-05 蓝牙模块通信
  - PC 通过蓝牙虚拟串口连接设备
  - Python 上位机支持状态显示、命令发送、自动刷新和串口日志查看

- **显示与供电**
  - OLED 显示当前状态
  - 支持外部电池/稳压模块供电
  - ST-LINK 仅用于下载和调试

---

## 2. 系统架构

```text
+------------------+        I2C        +----------------+
|                  | <---------------> |    MPU6050     |
|                  |                   | 姿态/运动采集   |
|                  |                   +----------------+
|                  |
|                  |        I2C        +----------------+
|   STM32F103C8T6  | <---------------> |      OLED      |
|                  |                   | 状态显示        |
|                  |                   +----------------+
|                  |
|                  |        SPI        +----------------+
|                  | <---------------> |    W25Qxx      |
|                  |                   | 外部日志 Flash  |
|                  |                   +----------------+
|                  |
|                  |       USART1      +----------------+
|                  | <---------------> |     HC-05      |
|                  |                   | 蓝牙串口模块    |
+------------------+                   +----------------+
                                               |
                                               | 蓝牙虚拟串口
                                               v
                                      +------------------+
                                      | Python 上位机     |
                                      | 状态监控/命令交互 |
                                      +------------------+
```

---

## 3. 硬件组成

| 模块 | 作用 |
|---|---|
| STM32F103C8T6 核心板 | 主控 |
| MPU6050 | 加速度与陀螺仪采集 |
| W25Qxx Flash | 异常日志掉电保存 |
| HC-05 | 蓝牙串口透传 |
| OLED | 状态显示 |
| ST-LINK | 下载与调试 |
| 外部电源模块 | 最终独立供电 |

---

## 4. 核心接线

### 4.1 HC-05

| HC-05 | STM32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| TXD | PA10 |
| RXD | PA9 |

### 4.2 OLED / MPU6050 共用软件 I2C

| 模块 | 引脚 | STM32 |
|---|---|---|
| OLED | SCL | PB8 |
| OLED | SDA | PB9 |
| MPU6050 | SCL | PB8 |
| MPU6050 | SDA | PB9 |
| OLED / MPU6050 | VCC | 3.3V |
| OLED / MPU6050 | GND | GND |

### 4.3 W25Qxx Flash

| W25Qxx | STM32 |
|---|---|
| CS | PA4 |
| SCK / CLK | PA5 |
| MISO / DO | PA6 |
| MOSI / DI | PA7 |
| VCC | 3.3V |
| GND | GND |

---

## 5. 软件模块划分

| 文件 | 作用 |
|---|---|
| `main.c` | 程序入口 |
| `app.c / app.h` | 应用层调度，串联采集、检测、显示、日志和串口 |
| `motion_detect.c / h` | 运动检测逻辑 |
| `blackbox_log.c / h` | 黑匣子日志读写、恢复和清空 |
| `W25Qxx.c / h` | Flash 底层驱动 |
| `MySPI.c / h` | SPI 通信底层 |
| `Serial.c / h` | USART1 串口收发和命令接收 |
| `hc05_monitor.py` | Python 上位机 |

> 需要根据实际情况确认：OLED、MPU6050 等驱动文件名以当前 Keil 工程为准。

---

## 6. 串口协议简表

### PC -> STM32

| 命令 | 作用 |
|---|---|
| `PING` | 测试设备在线 |
| `GET` | 获取当前状态 |
| `LAST` | 获取最后一条异常日志 |
| `CLR` | 清空日志 |
| `HELP` | 查看帮助 |
| `T yyyy-mm-dd hh:mm:ss` | 设置当前时间 |

### STM32 -> PC

| 响应 | 含义 |
|---|---|
| `OK,PONG` | PING 响应 |
| `STAT,...` | 当前状态 |
| `EVT,...` | 事件变化主动上报 |
| `LAST,...` | 最后一条日志 |
| `LAST,NONE` | 当前无日志 |
| `OK,CLR` | 清空成功 |
| `ERR,...` | 错误响应 |

详细协议见：[docs/PROTOCOL.md](docs/PROTOCOL.md)

---

## 7. Python 上位机

### 安装依赖

```bash
pip install pyserial
```

### 启动

```bash
python hc05_monitor.py
```

### 使用流程

1. 给 STM32 和 HC-05 上电。
2. Windows 蓝牙配对 HC-05。
3. 打开 Python 上位机。
4. 选择 HC-05 对应的 COM 口。
5. 波特率选择 `115200`。
6. 点击连接。
7. 点击 `PING`，应返回 `OK,PONG`。
8. 点击 `GET`，状态区更新。
9. 触发移动、倾斜或冲击，上位机应出现 `EVT,...`。
10. 点击 `LAST`，读取最后一条异常日志。
11. 点击 `CLR`，清空设备端 Flash 日志。

---

## 8. 已解决的典型问题

### 8.1 串口链路不清晰

开发早期曾混淆 STM32 USART、CH340 和 HC-05 的关系。后续明确：

```text
STM32 USART1 <-> HC-05 <-> 蓝牙虚拟串口 <-> Python 上位机
```

调试时按照硬件接线、波特率、串口接收、命令解析、上位机显示逐层排查。

### 8.2 单字节命令无法支撑复杂协议

早期命令是 `N / L / C / T`，后续升级为按行文本协议：

```text
PING\r\n
GET\r\n
LAST\r\n
CLR\r\n
HELP\r\n
T yyyy-mm-dd hh:mm:ss\r\n
```

### 8.3 W25Qxx 跨页写导致日志损坏

一条日志记录约 40 字节，W25Qxx 一页为 256 字节。第 7 条记录从偏移 240 开始写入：

```text
240 + 40 = 280
```

这会跨页。早期直接调用 `PageProgram` 整块写入，导致日志损坏。修复方式是按页分段写入，并增加写后回读校验。

### 8.4 Python 串口接收裂包

串口是字节流，不是消息包。上位机通过接收缓冲区和按 `\n` 分帧，解决 `STAT / LAST / EVT` 被拆开的情况。

### 8.5 运动检测抖动

早期单帧判断容易导致事件抖动。后续加入连续命中、连续释放和事件优先级，使 `MOVE / TILT / SHOCK` 状态切换更稳定。

详细复盘见：[docs/DEBUG_NOTES.md](docs/DEBUG_NOTES.md)

---

## 9. 项目文档

| 文档 | 内容 |
|---|---|
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 串口协议说明 |
| [docs/HARDWARE.md](docs/HARDWARE.md) | 硬件接线与供电 |
| [docs/DEBUG_NOTES.md](docs/DEBUG_NOTES.md) | 调试问题与复盘 |
| [docs/RESUME.md](docs/RESUME.md) | 简历与面试表达 |
| [docs/GITHUB_GUIDE.md](docs/GITHUB_GUIDE.md) | GitHub 仓库整理建议 |

---

## 10. 项目目录建议

```text
Blackbox/
├── README.md
├── .gitignore
├── hc05_monitor.py
├── docs/
│   ├── PROTOCOL.md
│   ├── HARDWARE.md
│   ├── DEBUG_NOTES.md
│   ├── RESUME.md
│   ├── GITHUB_GUIDE.md
│   └── images/
├── User/
├── Hardware/
├── Library/
├── Start/
└── System/
```

---

## 11. 后续扩展方向

- 引入 FreeRTOS，拆分采集、检测、通信、显示任务。
- 增加完整日志列表读取。
- 增加 CSV 导出。
- 增加 Pitch / Roll / AD / GD 曲线显示。
- 支持上位机在线配置检测阈值。
- 增加电池电压检测。
- 增加 Bootloader 和固件升级机制。
- 增加 RS485 / Modbus 通信接口。

---

## 12. 简历描述

基于 STM32F103C8T6 设计并实现多传感器事件黑匣子异常记录仪，完成 MPU6050 姿态采集、MOVE/TILT/SHOCK 事件检测、W25Qxx Flash 异常日志掉电保存、HC-05 蓝牙串口通信和 Python 上位机监控。项目中重点解决了串口协议升级、Python 串口接收裂包、W25Qxx 跨页写导致日志损坏、日志掉电恢复和传感器检测抖动等问题，形成了“采集 -> 检测 -> 记录 -> 掉电保存 -> 蓝牙传输 -> 上位机显示 -> 调试复盘”的完整嵌入式工程闭环。
