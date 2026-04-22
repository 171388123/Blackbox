import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports


class HC05MonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("HC-05 黑匣子最小上位机")
        self.root.geometry("900x600")

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

        self._build_ui()
        self.refresh_ports()
        self.root.after(100, self.poll_serial)

    def _build_ui(self):
        top = ttk.Frame(self.root, padding=10)
        top.pack(fill="x")

        ttk.Label(top, text="串口:").pack(side="left")
        self.port_combo = ttk.Combobox(
            top, textvariable=self.port_var, width=15, state="readonly"
        )
        self.port_combo.pack(side="left", padx=5)

        ttk.Button(top, text="刷新串口", command=self.refresh_ports).pack(side="left", padx=5)

        ttk.Label(top, text="波特率:").pack(side="left", padx=(20, 0))
        self.baud_combo = ttk.Combobox(
            top,
            textvariable=self.baud_var,
            width=10,
            state="readonly",
            values=["9600", "38400", "57600", "115200"],
        )
        self.baud_combo.pack(side="left", padx=5)

        ttk.Button(top, text="连接", command=self.connect_port).pack(side="left", padx=5)
        ttk.Button(top, text="断开", command=self.disconnect_port).pack(side="left", padx=5)

        ttk.Label(top, text="连接状态:").pack(side="left", padx=(20, 0))
        ttk.Label(top, textvariable=self.status_var).pack(side="left", padx=5)

        mid = ttk.LabelFrame(self.root, text="当前状态", padding=10)
        mid.pack(fill="x", padx=10, pady=10)

        row1 = ttk.Frame(mid)
        row1.pack(fill="x", pady=3)
        ttk.Label(row1, text="事件 EV:", width=12).pack(side="left")
        ttk.Label(row1, textvariable=self.ev_var, width=12).pack(side="left")
        ttk.Label(row1, text="Pitch:", width=12).pack(side="left")
        ttk.Label(row1, textvariable=self.pitch_var, width=12).pack(side="left")
        ttk.Label(row1, text="Roll:", width=12).pack(side="left")
        ttk.Label(row1, textvariable=self.roll_var, width=12).pack(side="left")

        row2 = ttk.Frame(mid)
        row2.pack(fill="x", pady=3)
        ttk.Label(row2, text="AD:", width=12).pack(side="left")
        ttk.Label(row2, textvariable=self.ad_var, width=12).pack(side="left")
        ttk.Label(row2, text="GD:", width=12).pack(side="left")
        ttk.Label(row2, textvariable=self.gd_var, width=12).pack(side="left")
        ttk.Label(row2, text="LOG:", width=12).pack(side="left")
        ttk.Label(row2, textvariable=self.log_var, width=12).pack(side="left")

        row3 = ttk.Frame(mid)
        row3.pack(fill="x", pady=3)
        ttk.Label(row3, text="TICK:", width=12).pack(side="left")
        ttk.Label(row3, textvariable=self.tick_var, width=12).pack(side="left")

        btn_frame = ttk.LabelFrame(self.root, text="快捷命令", padding=10)
        btn_frame.pack(fill="x", padx=10, pady=10)

        # 兼容你当前旧命令
        ttk.Button(btn_frame, text="旧命令 N", command=lambda: self.send_text("N")).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="旧命令 L", command=lambda: self.send_text("L")).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="旧命令 C", command=lambda: self.send_text("C")).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="旧命令 T", command=lambda: self.send_text("T")).pack(side="left", padx=5)

        # 预留新协议命令
        ttk.Button(btn_frame, text="PING", command=lambda: self.send_line("PING")).pack(side="left", padx=15)
        ttk.Button(btn_frame, text="GET", command=lambda: self.send_line("GET")).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="LAST", command=lambda: self.send_line("LAST")).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="CLR", command=lambda: self.send_line("CLR")).pack(side="left", padx=5)

        custom = ttk.LabelFrame(self.root, text="自定义发送", padding=10)
        custom.pack(fill="x", padx=10, pady=10)

        self.send_entry = ttk.Entry(custom)
        self.send_entry.pack(side="left", fill="x", expand=True, padx=5)
        ttk.Button(custom, text="按原样发送", command=self.send_custom_raw).pack(side="left", padx=5)
        ttk.Button(custom, text="按行发送", command=self.send_custom_line).pack(side="left", padx=5)

        log_frame = ttk.LabelFrame(self.root, text="串口日志", padding=10)
        log_frame.pack(fill="both", expand=True, padx=10, pady=10)

        self.text_log = tk.Text(log_frame, wrap="word")
        self.text_log.pack(side="left", fill="both", expand=True)

        scrollbar = ttk.Scrollbar(log_frame, orient="vertical", command=self.text_log.yview)
        scrollbar.pack(side="right", fill="y")
        self.text_log.config(yscrollcommand=scrollbar.set)

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])

    def connect_port(self):
        if self.ser and self.ser.is_open:
            messagebox.showinfo("提示", "串口已经连接")
            return

        port = self.port_var.get().strip()
        if not port:
            messagebox.showerror("错误", "请先选择串口")
            return

        baud = int(self.baud_var.get())

        try:
            self.ser = serial.Serial(port=port, baudrate=baud, timeout=0.05)
            self.status_var.set(f"已连接: {port} @ {baud}")
            self.append_log(f"[INFO] 串口已连接: {port} @ {baud}")
        except Exception as e:
            messagebox.showerror("连接失败", str(e))
            self.status_var.set("连接失败")

    def disconnect_port(self):
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
                    self.append_log("[INFO] 串口已断开")
            except Exception as e:
                self.append_log(f"[ERR] 断开异常: {e}")
        self.status_var.set("未连接")
        self.ser = None

    def send_text(self, text):
        if not self._check_connected():
            return
        try:
            self.ser.write(text.encode("utf-8"))
            self.append_log(f"[TX] {repr(text)}")
        except Exception as e:
            self.append_log(f"[ERR] 发送失败: {e}")

    def send_line(self, line):
        if not self._check_connected():
            return
        data = line + "\r\n"
        try:
            self.ser.write(data.encode("utf-8"))
            self.append_log(f"[TX] {repr(data)}")
        except Exception as e:
            self.append_log(f"[ERR] 发送失败: {e}")

    def send_custom_raw(self):
        text = self.send_entry.get()
        if not text:
            return
        self.send_text(text)

    def send_custom_line(self):
        text = self.send_entry.get()
        if not text:
            return
        self.send_line(text)

    def poll_serial(self):
        try:
            if self.ser and self.ser.is_open:
                data = self.ser.read_all()
                if data:
                    try:
                        text = data.decode("utf-8", errors="ignore")
                    except Exception:
                        text = str(data)

                    self.rx_buffer += text
                    self.append_log(f"[RX] {text}")

                    while "\n" in self.rx_buffer:
                        line, self.rx_buffer = self.rx_buffer.split("\n", 1)
                        line = line.strip()
                        if line:
                            self.parse_line(line)
        except Exception as e:
            self.append_log(f"[ERR] 接收异常: {e}")

        self.root.after(100, self.poll_serial)

    def parse_line(self, line):
        if not line.startswith("@ST,"):
            return

        content = line[4:]
        fields = content.split(",")

        kv = {}
        for item in fields:
            if "=" in item:
                key, value = item.split("=", 1)
                kv[key.strip()] = value.strip()

        self.ev_var.set(kv.get("EV", "---"))
        self.pitch_var.set(kv.get("PITCH", "---"))
        self.roll_var.set(kv.get("ROLL", "---"))
        self.ad_var.set(kv.get("AD", "---"))
        self.gd_var.set(kv.get("GD", "---"))
        self.log_var.set(kv.get("LOG", "---"))
        self.tick_var.set(kv.get("TICK", "---"))

    def append_log(self, text):
        self.text_log.insert("end", text + "\n")
        self.text_log.see("end")

    def _check_connected(self):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("提示", "请先连接串口")
            return False
        return True


if __name__ == "__main__":
    root = tk.Tk()
    app = HC05MonitorApp(root)
    root.mainloop()