# Blackbox 硬件连接说明

本文档记录 Blackbox 项目的硬件组成、电源方案和接线方式。

---

## 1. 硬件清单

| 硬件 | 作用 |
|---|---|
| STM32F103C8T6 核心板 | 主控 |
| MPU6050 | 加速度和陀螺仪采集 |
| W25Qxx Flash | 异常日志掉电保存 |
| HC-05 蓝牙模块 | 蓝牙串口通信 |
| OLED 显示屏 | 状态显示 |
| ST-LINK | 下载与调试 |
| 外部电池 / 稳压模块 | 独立供电 |
| 杜邦线 / 面包板 | 硬件连接 |

---

## 2. 推荐供电方案

最终演示建议：

```text
外部稳压 5V -> STM32 核心板 5V
外部电源 GND -> STM32 GND
STM32 3.3V -> 外设 VCC
```

注意：

- ST-LINK 只用于下载和调试。
- 外设尽量统一使用 3.3V。
- 不建议带电切换 3.3V / 5V。
- 不要把 5V 直接接到 W25Qxx、MPU6050、HC-05 裸模块。

---

## 3. HC-05 接线

| HC-05 | STM32 | 说明 |
|---|---|---|
| VCC | 3.3V | 电源 |
| GND | GND | 共地 |
| TXD | PA10 | HC-05 发送，STM32 接收 |
| RXD | PA9 | HC-05 接收，STM32 发送 |

说明：

- STM32 USART1_TX = PA9。
- STM32 USART1_RX = PA10。
- 串口要交叉连接。
- 如果 HC-05 是带稳压底板版本，供电电压需要按模块丝印确认；本项目默认按 3.3V 处理。

---

## 4. OLED 接线

| OLED | STM32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | PB8 |
| SDA | PB9 |

---

## 5. MPU6050 接线

| MPU6050 | STM32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | PB8 |
| SDA | PB9 |

说明：

- OLED 和 MPU6050 共用 PB8/PB9 软件 I2C。
- 共总线时建议全部按 3.3V 逻辑域处理。

---

## 6. W25Qxx Flash 接线

| W25Qxx | STM32 | 说明 |
|---|---|---|
| CS | PA4 | 片选，低电平有效 |
| SCK / CLK | PA5 | SPI 时钟 |
| MISO / DO | PA6 | Flash 输出到 STM32 |
| MOSI / DI | PA7 | STM32 输出到 Flash |
| VCC | 3.3V | 电源 |
| GND | GND | 共地 |

SPI 方向：

```text
STM32 PA5  -> W25Qxx CLK
STM32 PA7  -> W25Qxx DI
STM32 PA6  <- W25Qxx DO
STM32 PA4  -> W25Qxx CS
```

注意：

- `MOSI` 是主机输出、从机输入。
- `MISO` 是主机输入、从机输出。
- 不要把 `DI` 和 `DO` 接反。
- W25Qxx 不能接 5V。

---

## 7. ST-LINK 接线

| ST-LINK | STM32 |
|---|---|
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| 3.3V / VTref | STM32 3.3V |
| NRST | NRST，可选 |

说明：

- ST-LINK 用于下载和调试。
- 最终演示时建议使用外部电源供电。
- 如果 ST-LINK 保持连接，必须和目标板共地。

---

## 8. HC-05 和 CH340 的关系

HC-05 和 CH340 都走串口链路，但角色不同：

| 模块 | 作用 |
|---|---|
| CH340 | USB 转 TTL 串口，主要用于电脑有线调试 |
| HC-05 | 蓝牙串口透传，用于无线通信 |

不建议 HC-05 和 CH340 长期同时接在 USART1 的 PA9/PA10 上。  
如果同时连接，可能出现两个外设同时驱动串口线，导致数据冲突。

建议：

1. 使用 HC-05 时，拔掉 CH340 的 TX/RX。
2. 使用 CH340 调试时，拔掉 HC-05 的 TX/RX。
3. 需要长期共存时，增加跳帽或切换开关。

---

## 9. 上电前检查清单

- [ ] 外部电源 GND 与 STM32 GND 共地
- [ ] W25Qxx 接 3.3V
- [ ] HC-05 TXD 接 PA10
- [ ] HC-05 RXD 接 PA9
- [ ] W25Qxx DI/DO 没有接反
- [ ] OLED 和 MPU6050 共用 PB8/PB9
- [ ] HC-05 和 CH340 没有同时长期占用 USART1
- [ ] ST-LINK 只用于下载和调试
