import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import serial.tools.list_ports
import threading
import queue
import time

class App(tk.Tk):
    def __init__(self):
        super().__init__()

        self.title("RP2040 CC1101 Signal Cloner")
        self.geometry("800x600")

        self.serial_port = None
        self.message_queue = queue.Queue()

        # --- UI Setup ---
        self.create_widgets()

        # Start checking the queue for messages from the serial thread
        self.after(100, self.process_serial_messages)

        # Populate serial ports dropdown
        self.update_serial_ports()

    def create_widgets(self):
        # --- Connection Frame ---
        conn_frame = ttk.LabelFrame(self, text="Connection", padding="10")
        conn_frame.pack(fill="x", padx=10, pady=5)

        self.port_label = ttk.Label(conn_frame, text="Serial Port:")
        self.port_label.pack(side="left", padx=5)

        self.port_var = tk.StringVar()
        self.port_menu = ttk.Combobox(conn_frame, textvariable=self.port_var, state="readonly")
        self.port_menu.pack(side="left", fill="x", expand=True, padx=5)

        self.connect_button = ttk.Button(conn_frame, text="Connect", command=self.toggle_connection)
        self.connect_button.pack(side="left", padx=5)

        self.status_label = ttk.Label(conn_frame, text="Status: Disconnected", foreground="red")
        self.status_label.pack(side="left", padx=5)

        # --- Log Frame ---
        log_frame = ttk.LabelFrame(self, text="Device Log", padding="10")
        log_frame.pack(fill="both", expand=True, padx=10, pady=5)

        self.log_text = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, state="disabled")
        self.log_text.pack(fill="both", expand=True)

        # --- Control Frame ---
        ctrl_frame = ttk.LabelFrame(self, text="Controls", padding="10")
        ctrl_frame.pack(fill="x", padx=10, pady=5)

        self.pin_check_button = ttk.Button(ctrl_frame, text="Check Pins", command=lambda: self.send_command('p'))
        self.pin_check_button.pack(side="left", padx=5)

        self.capture_button = ttk.Button(ctrl_frame, text="Capture Signal", command=lambda: self.send_command('c'))
        self.capture_button.pack(side="left", padx=5)

        # --- Transmit Frame ---
        tx_frame = ttk.LabelFrame(self, text="Transmit", padding="10")
        tx_frame.pack(fill="x", padx=10, pady=5)

        self.tx_label = ttk.Label(tx_frame, text="Pulse Data (us, comma-separated):")
        self.tx_label.pack(side="left", padx=5)

        self.tx_entry = ttk.Entry(tx_frame)
        self.tx_entry.pack(side="left", fill="x", expand=True, padx=5)

        self.transmit_button = ttk.Button(tx_frame, text="Transmit Signal", command=self.transmit_signal)
        self.transmit_button.pack(side="left", padx=5)

        self.disable_controls()

    def update_serial_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_menu['values'] = ports
        if ports:
            self.port_var.set(ports[0])

    def toggle_connection(self):
        if self.serial_port and self.serial_port.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_var.get()
        if not port:
            self.log_message("System: No port selected.")
            return
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            self.log_message(f"System: Connecting to {port}...")

            # Start the serial reader thread
            self.reader_thread = threading.Thread(target=self.read_from_port, daemon=True)
            self.reader_thread.start()

            self.connect_button.config(text="Disconnect")
            self.status_label.config(text="Status: Connected", foreground="green")
            self.enable_controls()
            self.log_message(f"System: Connected successfully.")

        except serial.SerialException as e:
            self.log_message(f"System: Failed to connect - {e}")

    def disconnect(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            self.log_message("System: Disconnected.")
        self.connect_button.config(text="Connect")
        self.status_label.config(text="Status: Disconnected", foreground="red")
        self.disable_controls()

    def read_from_port(self):
        while self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line:
                    self.message_queue.put(line)
            except (serial.SerialException, TypeError):
                break
        # Port closed or error, put a final message
        self.message_queue.put("STOP_LISTENING")

    def process_serial_messages(self):
        while not self.message_queue.empty():
            message = self.message_queue.get_nowait()
            if message == "STOP_LISTENING":
                # This is a signal that the port was closed.
                # We might already be in a disconnected state, but this ensures it.
                if self.serial_port:
                    self.disconnect()
                return
            self.log_message(f"DEV: {message}")
        self.after(100, self.process_serial_messages)

    def log_message(self, message):
        self.log_text.config(state="normal")
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.log_text.config(state="disabled")

    def send_command(self, command):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.write(command.encode('utf-8'))
            self.log_message(f"System: Sent '{command}' command.")
        else:
            self.log_message("System: Not connected.")

    def transmit_signal(self):
        data = self.tx_entry.get()
        if not data:
            self.log_message("System: Transmit data is empty.")
            return

        self.send_command('t')
        time.sleep(0.1) # Give device time to process 't' command

        if self.serial_port and self.serial_port.is_open:
            self.serial_port.write(f"{data}\n".encode('utf-8'))
            self.log_message("System: Sent transmit data.")

    def disable_controls(self):
        self.pin_check_button.config(state="disabled")
        self.capture_button.config(state="disabled")
        self.transmit_button.config(state="disabled")
        self.tx_entry.config(state="disabled")

    def enable_controls(self):
        self.pin_check_button.config(state="normal")
        self.capture_button.config(state="normal")
        self.transmit_button.config(state="normal")
        self.tx_entry.config(state="normal")

    def on_closing(self):
        self.disconnect()
        self.destroy()

if __name__ == "__main__":
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
