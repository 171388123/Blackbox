import tkinter as tk
from tkinter import ttk, messagebox
from tkinter.scrolledtext import ScrolledText
import serial
import serial.tools.list_ports


class HC05MonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("HC-05 黑匣子最小上位机")
        self.root.geometry("1200x800")

        self.ser = None
        self.rx_buffer = ""

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value="115200")
        self.status_var = tk.StringVar(value="未连接")

        self.ev_var = tk.StringVar(value="---")
        self.pitch_var = tk.StringVar(value="---")
        self.roll_var = tk.StringVar(value="---")
        self.ad_var = tk.StringVar(value="---")
        self.gd_var = tk.StringVar(value="---")
        self.log_var = tk.StringVar(value="---")
        self.tick_var = tk.StringVar(value="---")
        self.time_var = tk.StringVar(value="---")

        self.auto_poll_var = tk.BooleanVar(value=True)
        self.silent_stat_var = tk.BooleanVar(value=True)
        self.poll_interval_var = tk.StringVar(value="1000")

        # 记录 GET 请求对应的响应处理模式
        # "log"    -> 收到 STAT 后写日志
        # "silent" -> 收到 STAT 后只更新状态区，不写日志
        self.get_mode_queue = []

        self.build_ui()
        self.refresh_ports()

        self.root.after(100, self.poll_serial)
        self.root.after(1000, self.auto_poll_task)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    # =========================
    # UI
    # =========================
    def build_ui(self):
        top = ttk.Frame(self.root, padding=10)
        top.pack(fill="x")

        ttk.Label(top, text="串口:").pack(side="left")
        self.port_combo = ttk.Combobox(
            top,
            textvariable=self.port_var,
            width=12,
            state="readonly"
        )
        self.port_combo.pack(side="left", padx=(5, 10))

        self.btn_refresh = ttk.Button(top, text="刷新串口", command=self.refresh_ports)
        self.btn_refresh.pack(side="left", padx=5)

        ttk.Label(top, text="波特率:").pack(side="left", padx=(20, 0))
        self.baud_combo = ttk.Combobox(
            top,
            textvariable=self.baud_var,
            width=10,
            state="readonly",
            values=["9600", "19200", "38400", "57600", "115200"]
        )
        self.baud_combo.pack(side="left", padx=(5, 10))

        self.btn_connect = ttk.Button(top, text="连接", command=self.connect_port)
        self.btn_connect.pack(side="left", padx=5)

        self.btn_disconnect = ttk.Button(
            top,
            text="断开",
            command=self.disconnect_port,
            state="disabled"
        )
        self.btn_disconnect.pack(side="left", padx=5)

        ttk.Label(top, text="连接状态:").pack(side="left", padx=(20, 5))
        ttk.Label(top, textvariable=self.status_var).pack(side="left")

        auto_frame = ttk.LabelFrame(self.root, text="自动刷新", padding=12)
        auto_frame.pack(fill="x", padx=10, pady=(0, 10))

        self.chk_auto_poll = ttk.Checkbutton(
            auto_frame,
            text="自动刷新 GET",
            variable=self.auto_poll_var
        )
        self.chk_auto_poll.pack(side="left", padx=(0, 15))

        ttk.Label(auto_frame, text="周期(ms):").pack(side="left")
        self.poll_combo = ttk.Combobox(
            auto_frame,
            textvariable=self.poll_interval_var,
            width=8,
            state="readonly",
            values=["500", "1000", "2000"]
        )
        self.poll_combo.pack(side="left", padx=(5, 15))

        self.chk_silent_stat = ttk.Checkbutton(
            auto_frame,
            text="周期 STAT 静默更新",
            variable=self.silent_stat_var
        )
        self.chk_silent_stat.pack(side="left", padx=(0, 15))

        ttk.Label(
            auto_frame,
            text="说明：自动轮询得到的 STAT 默认只更新状态区，不刷日志"
        ).pack(side="left", padx=(10, 0))

        state_frame = ttk.LabelFrame(self.root, text="当前状态", padding=12)
        state_frame.pack(fill="x", padx=10, pady=(0, 10))

        row1 = ttk.Frame(state_frame)
        row1.pack(fill="x", pady=3)

        ttk.Label(row1, text="事件 EV:", width=12).pack(side="left")
        ttk.Label(row1, textvariable=self.ev_var, width=18).pack(side="left")

        ttk.Label(row1, text="Pitch:", width=12).pack(side="left")
        ttk.Label(row1, textvariable=self.pitch_var, width=18).pack(side="left")

        ttk.Label(row1, text="Roll:", width=12).pack(side="left")
        ttk.Label(row1, textvariable=self.roll_var, width=18).pack(side="left")

        row2 = ttk.Frame(state_frame)
        row2.pack(fill="x", pady=3)

        ttk.Label(row2, text="AD:", width=12).pack(side="left")
        ttk.Label(row2, textvariable=self.ad_var, width=18).pack(side="left")

        ttk.Label(row2, text="GD:", width=12).pack(side="left")
        ttk.Label(row2, textvariable=self.gd_var, width=18).pack(side="left")

        ttk.Label(row2, text="LOG:", width=12).pack(side="left")
        ttk.Label(row2, textvariable=self.log_var, width=18).pack(side="left")

        row3 = ttk.Frame(state_frame)
        row3.pack(fill="x", pady=3)

        ttk.Label(row3, text="TICK:", width=12).pack(side="left")
        ttk.Label(row3, textvariable=self.tick_var, width=18).pack(side="left")

        ttk.Label(row3, text="TIME:", width=12).pack(side="left")
        ttk.Label(row3, textvariable=self.time_var, width=30).pack(side="left")

        cmd_frame = ttk.LabelFrame(self.root, text="快捷命令", padding=12)
        cmd_frame.pack(fill="x", padx=10, pady=(0, 10))

        self.btn_old_n = ttk.Button(cmd_frame, text="旧命令 N", command=lambda: self.send_text("N"))
        self.btn_old_n.pack(side="left", padx=5)

        self.btn_old_l = ttk.Button(cmd_frame, text="旧命令 L", command=lambda: self.send_text("L"))
        self.btn_old_l.pack(side="left", padx=5)

        self.btn_old_c = ttk.Button(cmd_frame, text="旧命令 C", command=lambda: self.send_text("C"))
        self.btn_old_c.pack(side="left", padx=5)

        self.btn_help = ttk.Button(cmd_frame, text="HELP", command=lambda: self.send_line("HELP"))
        self.btn_help.pack(side="left", padx=20)

        self.btn_ping = ttk.Button(cmd_frame, text="PING", command=lambda: self.send_line("PING"))
        self.btn_ping.pack(side="left", padx=5)

        self.btn_get = ttk.Button(cmd_frame, text="GET", command=self.send_get_manual)
        self.btn_get.pack(side="left", padx=5)

        self.btn_last = ttk.Button(cmd_frame, text="LAST", command=lambda: self.send_line("LAST"))
        self.btn_last.pack(side="left", padx=5)

        self.btn_clr = ttk.Button(cmd_frame, text="CLR", command=lambda: self.send_line("CLR"))
        self.btn_clr.pack(side="left", padx=5)

        custom_frame = ttk.LabelFrame(self.root, text="自定义发送", padding=12)
        custom_frame.pack(fill="x", padx=10, pady=(0, 10))

        self.custom_entry = ttk.Entry(custom_frame)
        self.custom_entry.pack(side="left", fill="x", expand=True, padx=(0, 10))

        self.btn_send_raw = ttk.Button(custom_frame, text="按原样发送", command=self.send_custom_raw)
        self.btn_send_raw.pack(side="left", padx=5)

        self.btn_send_line = ttk.Button(custom_frame, text="按行发送", command=self.send_custom_line)
        self.btn_send_line.pack(side="left", padx=5)

        log_frame = ttk.LabelFrame(self.root, text="串口日志", padding=12)
        log_frame.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        log_top = ttk.Frame(log_frame)
        log_top.pack(fill="x", pady=(0, 8))

        self.btn_clear_log = ttk.Button(log_top, text="一键清空串口日志", command=self.clear_log_text)
        self.btn_clear_log.pack(side="right")

        self.log_text = ScrolledText(log_frame, font=("Consolas", 12), wrap="word")
        self.log_text.pack(fill="both", expand=True)
        self.log_text.config(state="disabled")

    # =========================
    # 串口基础
    # =========================
    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo["values"] = ports

        if ports:
            if self.port_var.get() not in ports:
                self.port_var.set(ports[0])
        else:
            self.port_var.set("")

    def connect_port(self):
        if self.ser and self.ser.is_open:
            return

        port = self.port_var.get().strip()
        baud = self.baud_var.get().strip()

        if not port:
            messagebox.showwarning("提示", "请先选择串口")
            return

        try:
            self.ser = serial.Serial(port=port, baudrate=int(baud), timeout=0.05)
            self.rx_buffer = ""
            self.get_mode_queue.clear()

            self.status_var.set(f"已连接: {port} @ {baud}")
            self.append_log(f"[INFO] 串口已连接: {port} @ {baud}")
            self.set_connected_ui(True)
        except Exception as e:
            self.ser = None
            messagebox.showerror("连接失败", str(e))
            self.append_log(f"[ERR] 连接失败: {e}")

    def disconnect_port(self):
        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
                self.append_log("[INFO] 串口已断开")
        except Exception as e:
            self.append_log(f"[ERR] 断开异常: {e}")
        finally:
            self.ser = None
            self.rx_buffer = ""
            self.get_mode_queue.clear()
            self.status_var.set("未连接")
            self.set_connected_ui(False)

    def set_connected_ui(self, connected: bool):
        if connected:
            self.btn_connect.config(state="disabled")
            self.btn_disconnect.config(state="normal")
            self.port_combo.config(state="disabled")
            self.baud_combo.config(state="disabled")
            self.btn_refresh.config(state="disabled")
        else:
            self.btn_connect.config(state="normal")
            self.btn_disconnect.config(state="disabled")
            self.port_combo.config(state="readonly")
            self.baud_combo.config(state="readonly")
            self.btn_refresh.config(state="normal")

    def on_close(self):
        self.disconnect_port()
        self.root.destroy()

    # =========================
    # 发送
    # =========================
    def send_text(self, text: str, log_tx: bool = True):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("提示", "串口未连接")
            return False

        try:
            data = text.encode("utf-8")
            self.ser.write(data)
            if log_tx:
                self.append_log(f"[TX] {repr(text)}")
            return True
        except Exception as e:
            self.append_log(f"[ERR] 发送失败: {e}")
            return False

    def send_line(self, line: str, log_tx: bool = True):
        return self.send_text(line + "\r\n", log_tx=log_tx)

    def send_get_manual(self):
        self.get_mode_queue.append("log")
        self.send_line("GET", log_tx=True)

    def send_get_auto(self):
        if len(self.get_mode_queue) >= 3:
            # 避免设备异常时队列一直堆积
            return

        mode = "silent" if self.silent_stat_var.get() else "log"
        self.get_mode_queue.append(mode)
        self.send_line("GET", log_tx=False)

    def send_custom_raw(self):
        text = self.custom_entry.get()
        if not text:
            return
        self.send_text(text)

    def send_custom_line(self):
        text = self.custom_entry.get().strip()
        if not text:
            return
        self.send_line(text)

    # =========================
    # 自动轮询
    # =========================
    def get_poll_interval_ms(self):
        try:
            value = int(self.poll_interval_var.get().strip())
            if value < 100:
                value = 100
            return value
        except Exception:
            return 1000

    def auto_poll_task(self):
        try:
            if self.auto_poll_var.get():
                if self.ser and self.ser.is_open:
                    self.send_get_auto()
        except Exception as e:
            self.append_log(f"[ERR] 自动刷新异常: {e}")

        self.root.after(self.get_poll_interval_ms(), self.auto_poll_task)

    # =========================
    # 接收
    # =========================
    def poll_serial(self):
        try:
            if self.ser and self.ser.is_open:
                data = self.ser.read_all()
                if data:
                    text = data.decode("utf-8", errors="ignore")
                    self.rx_buffer += text

                    while "\n" in self.rx_buffer:
                        line, self.rx_buffer = self.rx_buffer.split("\n", 1)
                        line = line.rstrip("\r").strip()
                        if not line:
                            continue

                        self.handle_rx_line(line)

        except Exception as e:
            self.append_log(f"[ERR] 接收异常: {e}")

        self.root.after(100, self.poll_serial)

    def handle_rx_line(self, line: str):
        should_log = True

        if line.startswith("STAT,"):
            mode = "log"
            if self.get_mode_queue:
                mode = self.get_mode_queue.pop(0)

            kv = self.parse_kv_fields(line[5:])
            self.update_state_view(kv)

            if mode == "silent":
                should_log = False

        elif line.startswith("EVT,"):
            kv = self.parse_kv_fields(line[4:])
            self.update_state_view(kv)
            should_log = True

        elif line.startswith("OK,TIME="):
            self.time_var.set(line[len("OK,TIME="):].strip())
            should_log = True

        elif line.startswith("LAST,"):
            should_log = True

        elif line == "LAST,NONE":
            should_log = True

        elif line.startswith("OK,"):
            should_log = True

        elif line.startswith("ERR,"):
            should_log = True

        else:
            # [INIT] / [BOOT] / [CMD] / [EV] / [LOG] 等原始调试行
            should_log = True

        if should_log:
            self.append_log(f"[RX] {line}")

    # =========================
    # 协议解析
    # =========================
    def parse_kv_fields(self, payload: str):
        kv = {}
        for item in payload.split(","):
            if "=" in item:
                key, value = item.split("=", 1)
                kv[key.strip()] = value.strip()
        return kv

    def update_state_view(self, kv: dict):
        self.ev_var.set(kv.get("EV", self.ev_var.get()))
        self.pitch_var.set(kv.get("PITCH", self.pitch_var.get()))
        self.roll_var.set(kv.get("ROLL", self.roll_var.get()))
        self.ad_var.set(kv.get("AD", self.ad_var.get()))
        self.gd_var.set(kv.get("GD", self.gd_var.get()))
        self.log_var.set(kv.get("LOG", self.log_var.get()))
        self.tick_var.set(kv.get("TICK", self.tick_var.get()))
        self.time_var.set(kv.get("TIME", self.time_var.get()))

    # =========================
    # 日志
    # =========================
    def append_log(self, text: str):
        self.log_text.config(state="normal")
        self.log_text.insert("end", text + "\n")
        self.log_text.see("end")
        self.log_text.config(state="disabled")

    def clear_log_text(self):
        self.log_text.config(state="normal")
        self.log_text.delete("1.0", "end")
        self.log_text.config(state="disabled")


def main():
    root = tk.Tk()
    app = HC05MonitorApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()