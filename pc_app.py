import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import serial.tools.list_ports
import threading
import time

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("RP2040 CC1101 Signal Cloner")
        self.geometry("600x600") # Increased height for new frame

        self.serial_port = None
        self.thread = None
        self.running = False
        self.scanning = False

        # --- GUI Elements ---

        # Frame for connection management
        conn_frame = ttk.LabelFrame(self, text="Connection")
        conn_frame.pack(padx=10, pady=5, fill="x")

        self.port_label = ttk.Label(conn_frame, text="Port:")
        self.port_label.pack(side=tk.LEFT, padx=5, pady=5)

        self.port_combobox = ttk.Combobox(conn_frame)
        self.port_combobox['values'] = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combobox.pack(side=tk.LEFT, padx=5, pady=5, expand=True, fill="x")

        self.connect_button = ttk.Button(conn_frame, text="Connect", command=self.connect_serial)
        self.connect_button.pack(side=tk.LEFT, padx=5, pady=5)

        self.disconnect_button = ttk.Button(conn_frame, text="Disconnect", command=self.disconnect_serial, state=tk.DISABLED)
        self.disconnect_button.pack(side=tk.LEFT, padx=5, pady=5)

        # Frame for controls
        control_frame = ttk.LabelFrame(self, text="Controls & Debug")
        control_frame.pack(padx=10, pady=5, fill="x")

        self.start_capture_button = ttk.Button(control_frame, text="Start Capture", command=lambda: self.send_command("C"), state=tk.DISABLED)
        self.start_capture_button.pack(side=tk.LEFT, padx=5, pady=5)

        self.start_rssi_button = ttk.Button(control_frame, text="Start RSSI Scan", command=lambda: self.send_command("S"), state=tk.DISABLED)
        self.start_rssi_button.pack(side=tk.LEFT, padx=5, pady=5)

        self.stop_button = ttk.Button(control_frame, text="Stop", command=lambda: self.send_command("E"), state=tk.DISABLED)
        self.stop_button.pack(side=tk.LEFT, padx=5, pady=5)

        self.dump_regs_button = ttk.Button(control_frame, text="Dump Registers", command=lambda: self.send_command("D"), state=tk.DISABLED)
        self.dump_regs_button.pack(side=tk.LEFT, padx=5, pady=5)

        # Frame for Frequency Scanner
        scan_frame = ttk.LabelFrame(self, text="Frequency Scanner")
        scan_frame.pack(padx=10, pady=5, fill="x")

        ttk.Label(scan_frame, text="Start (KHz):").grid(row=0, column=0, padx=5, pady=2, sticky="w")
        self.start_freq_entry = ttk.Entry(scan_frame)
        self.start_freq_entry.insert(0, "433000")
        self.start_freq_entry.grid(row=0, column=1, padx=5, pady=2, sticky="ew")

        ttk.Label(scan_frame, text="End (KHz):").grid(row=1, column=0, padx=5, pady=2, sticky="w")
        self.end_freq_entry = ttk.Entry(scan_frame)
        self.end_freq_entry.insert(0, "435000")
        self.end_freq_entry.grid(row=1, column=1, padx=5, pady=2, sticky="ew")

        ttk.Label(scan_frame, text="Step (KHz):").grid(row=0, column=2, padx=5, pady=2, sticky="w")
        self.step_freq_entry = ttk.Entry(scan_frame)
        self.step_freq_entry.insert(0, "50")
        self.step_freq_entry.grid(row=0, column=3, padx=5, pady=2, sticky="ew")

        self.start_scan_button = ttk.Button(scan_frame, text="Start Scan", command=self.start_frequency_scan, state=tk.DISABLED)
        self.start_scan_button.grid(row=1, column=2, columnspan=2, padx=5, pady=2, sticky="ew")

        scan_frame.columnconfigure(1, weight=1)
        scan_frame.columnconfigure(3, weight=1)

        # Log area
        log_frame = ttk.LabelFrame(self, text="Log")
        log_frame.pack(padx=10, pady=10, expand=True, fill="both")

        self.log_area = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, state=tk.DISABLED)
        self.log_area.pack(expand=True, fill="both")

    def log(self, message):
        self.log_area.config(state=tk.NORMAL)
        self.log_area.insert(tk.END, message + "\n")
        self.log_area.see(tk.END)
        self.log_area.config(state=tk.DISABLED)

    def connect_serial(self):
        port = self.port_combobox.get()
        if not port:
            self.log("Error: No port selected.")
            return
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            self.log(f"Connected to {port}.")
            self.set_control_state(tk.NORMAL)
            self.connect_button.config(state=tk.DISABLED)
            self.disconnect_button.config(state=tk.NORMAL)
            self.running = True
            self.thread = threading.Thread(target=self.read_serial)
            self.thread.daemon = True
            self.thread.start()
        except serial.SerialException as e:
            self.log(f"Error connecting: {e}")

    def disconnect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.running = False
            if self.thread:
                self.thread.join(timeout=1)
            self.serial_port.close()
            self.log("Disconnected.")
        self.set_control_state(tk.DISABLED)
        self.connect_button.config(state=tk.NORMAL)
        self.disconnect_button.config(state=tk.DISABLED)

    def set_control_state(self, state):
        self.start_capture_button.config(state=state)
        self.start_rssi_button.config(state=state)
        self.stop_button.config(state=state)
        self.dump_regs_button.config(state=state)
        self.start_scan_button.config(state=state)

    def read_serial(self):
        while self.running:
            try:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode('utf-8').strip()
                    if line:
                        if "OK: Frequency scan finished" in line:
                            self.scanning = False
                            self.set_control_state(tk.NORMAL)
                        self.log(f"Pico: {line}")
            except (serial.SerialException, TypeError):
                self.log("Error reading from serial port.")
                self.disconnect_serial()
                break
            except Exception as e:
                self.log(f"An error occurred: {e}")
            time.sleep(0.01)

    def send_command(self, command):
        if self.serial_port and self.serial_port.is_open:
            full_command = command + "\n"
            self.serial_port.write(full_command.encode('utf-8'))
            self.log(f"Sent: {command}")
        else:
            self.log("Not connected.")

    def start_frequency_scan(self):
        try:
            start = int(self.start_freq_entry.get())
            end = int(self.end_freq_entry.get())
            step = int(self.step_freq_entry.get())
            if start >= end or step <= 0:
                self.log("Error: Invalid frequency range or step.")
                return
            self.scanning = True
            self.set_control_state(tk.DISABLED) # Disable other controls
            self.stop_button.config(state=tk.NORMAL) # Keep stop button active
            self.send_command(f"F,{start},{end},{step}")
        except ValueError:
            self.log("Error: Frequency values must be integers.")

    def on_closing(self):
        self.disconnect_serial()
        self.destroy()

if __name__ == "__main__":
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
