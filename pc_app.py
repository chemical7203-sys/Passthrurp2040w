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
        self.geometry("700x500")

        self.serial_port = None
        self.thread = None
        self.running = False

        # --- GUI Elements ---

        # Frame for connection management
        conn_frame = ttk.LabelFrame(self, text="Connection")
        conn_frame.pack(padx=10, pady=10, fill="x")

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
        control_frame = ttk.LabelFrame(self, text="Capture & Clone")
        control_frame.pack(padx=10, pady=5, fill="x", expand=False)

        self.start_capture_button = ttk.Button(control_frame, text="Start Capture", command=lambda: self.send_command("C"), state=tk.DISABLED)
        self.start_capture_button.pack(side=tk.LEFT, padx=5, pady=5)

        self.tx_entry = ttk.Entry(control_frame)
        self.tx_entry.pack(side=tk.LEFT, padx=5, pady=5, expand=True, fill="x")

        self.transmit_button = ttk.Button(control_frame, text="Transmit Captured Signal", command=self.transmit_data, state=tk.DISABLED)
        self.transmit_button.pack(side=tk.LEFT, padx=5, pady=5)

        # Log area
        log_frame = ttk.LabelFrame(self, text="Log")
        log_frame.pack(padx=10, pady=10, expand=True, fill="both")

        self.log_area = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, state=tk.DISABLED, font=("Courier New", 9))
        self.log_area.pack(expand=True, fill="both")

    def log(self, message, end="\n"):
        self.log_area.config(state=tk.NORMAL)
        self.log_area.insert(tk.END, message + end)
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
            self.start_capture_button.config(state=tk.NORMAL)
            self.transmit_button.config(state=tk.NORMAL)
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
        self.start_capture_button.config(state=tk.DISABLED)
        self.transmit_button.config(state=tk.DISABLED)
        self.connect_button.config(state=tk.NORMAL)
        self.disconnect_button.config(state=tk.DISABLED)

    def read_serial(self):
        buffer = ""
        while self.running:
            try:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    data = self.serial_port.read(self.serial_port.in_waiting).decode('utf-8', errors='ignore')
                    buffer += data

                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        self.process_line(line.strip())

            except (serial.SerialException, TypeError):
                self.log("Error reading from serial port.")
                break
            except Exception as e:
                self.log(f"An error occurred: {e}")
            time.sleep(0.05)

        # Ensure UI is disabled after thread stops
        if not self.running:
            self.disconnect_serial()

    def process_line(self, line):
        if not line:
            return

        self.log(f"Pico: {line}")
        if line.startswith("PULSE:"):
            pulse_data = line.replace("PULSE:", "").strip()
            self.tx_entry.delete(0, tk.END)
            self.tx_entry.insert(0, pulse_data)
            self.log("-> Copied pulse data to transmit box.")

    def send_command(self, command):
        if self.serial_port and self.serial_port.is_open:
            full_command = command + "\n"
            self.serial_port.write(full_command.encode('utf-8'))
            self.log(f"Sent: {command}")
        else:
            self.log("Not connected.")

    def transmit_data(self):
        data = self.tx_entry.get().strip()
        if not data:
            self.log("Error: Transmit data is empty.")
            return

        if not all(c in '0123456789,' for c in data):
            self.log("Error: Invalid characters in pulse data. Should be numbers and commas.")
            return

        self.send_command(f"P,{data}")

    def on_closing(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        self.destroy()

if __name__ == "__main__":
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
