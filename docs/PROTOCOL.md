# Blackbox 串口协议说明

本文档说明 Blackbox 项目中 PC 上位机与 STM32 之间的串口通信协议。协议同时适用于 USB 串口调试和 HC-05 蓝牙透明串口通信。

---

## 1. 协议设计目标

本项目采用 ASCII 文本协议，目标是：

1. 便于串口助手直接调试。
2. 便于 Python 上位机解析。
3. 便于后续扩展字段。
4. 便于通过 HC-05 蓝牙透明串口传输。
5. 在项目阶段优先保证可读性和可调试性。

协议格式：

```text
命令 + \r\n
响应 + \r\n
```

---

## 2. PC -> STM32 命令

| 命令 | 参数 | 作用 |
|---|---|---|
| `PING` | 无 | 测试设备是否在线 |
| `GET` | 无 | 获取当前状态 |
| `LAST` | 无 | 获取最后一条异常日志 |
| `CLR` | 无 | 清空 Flash 日志 |
| `HELP` | 无 | 查看命令帮助 |
| `T yyyy-mm-dd hh:mm:ss` | 时间字符串 | 设置当前时间 |

---

## 3. STM32 -> PC 响应

| 响应 | 含义 |
|---|---|
| `OK,PONG` | PING 响应 |
| `STAT,...` | 当前状态 |
| `EVT,...` | 事件变化主动上报 |
| `LAST,...` | 最后一条日志 |
| `LAST,NONE` | 当前无日志 |
| `OK,CLR` | 清空日志成功 |
| `OK,TIME=...` | 时间设置成功 |
| `ERR,...` | 错误响应 |

---

## 4. 示例

### PING

```text
PING\r\n
```

响应：

```text
OK,PONG\r\n
```

### GET

```text
GET\r\n
```

响应：

```text
STAT,TIME=2026-04-22_16:08:58,TICK=538661,EV=NORM,PITCH=0.2,ROLL=2.0,AD=43,GD=10,LOG=2\r\n
```

### EVT

`EVT` 是 STM32 主动上报，不需要 PC 先发命令。

```text
EVT,TIME=2026-04-22_16:09:02,TICK=542949,EV=MOVE,PITCH=5.3,ROLL=12.5,AD=4562,GD=2483,LOG=2\r\n
```

### LAST

```text
LAST\r\n
```

有日志：

```text
LAST,SEQ=1,TIME=2026-04-22_16:00:26,TICK=26231,EV=TILT,PITCH=0.0,ROLL=27.5,AD=2727,GD=2373\r\n
```

无日志：

```text
LAST,NONE\r\n
```

---

## 5. 字段说明

| 字段 | 含义 |
|---|---|
| `TIME` | 当前时间或日志时间 |
| `TICK` | 系统运行毫秒计时 |
| `EV` | 当前事件状态 |
| `PITCH` | Pitch 姿态角 |
| `ROLL` | Roll 姿态角 |
| `AD` | 加速度变化量 |
| `GD` | 陀螺仪变化量 |
| `LOG` | 当前日志数量 |
| `SEQ` | 日志序号 |

---

## 6. 事件状态

| 事件 | 含义 |
|---|---|
| `NORM` / `NONE` | 正常状态 |
| `MOVE` | 移动 |
| `TILT` | 倾斜 |
| `SHOCK` | 冲击 |
| `UNK` | 未知 |

> 需要根据实际情况确认：当前代码中正常状态显示可能是 `NORM`，宏定义名称可能是 `MOTION_EVENT_NONE`。

---

## 7. 分帧规则

串口是字节流，不是消息包。不能假设一次 `read()` 就能读到一整条响应。

本协议规定：

- PC 每条命令以 `\r\n` 结尾。
- STM32 每条响应以 `\r\n` 结尾。
- Python 上位机按 `\n` 分帧。
- 分帧后去掉行尾 `\r` 和空白字符。
- 只有拿到完整一行后才解析。

Python 上位机分帧思路：

```python
rx_buffer += text

while "\n" in rx_buffer:
    line, rx_buffer = rx_buffer.split("\n", 1)
    line = line.rstrip("\r").strip()
    if line:
        handle_rx_line(line)
```

---

## 8. 为什么使用文本协议

当前项目选择文本协议，而不是二进制协议，原因是：

1. 调试直观。
2. 可以直接用串口助手观察。
3. Python 解析简单。
4. 字段扩展方便。
5. 更适合本科项目展示和调试复盘。

后续如果对效率和可靠性要求更高，可以扩展为：

```text
帧头 + 长度 + 命令字 + Payload + 校验
```
