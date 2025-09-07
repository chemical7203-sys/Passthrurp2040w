import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import queue
import platform

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Pico CC1101 Cloner Interface")
        self.geometry("700x500")

        self.serial_port = None
        self.port_thread = None
        self.stop_thread = False
        self.data_queue = queue.Queue()

        # --- Top Frame for Connection ---
        connection_frame = ttk.LabelFrame(self, text="Connection")
        connection_frame.pack(padx=10, pady=10, fill="x")

        ttk.Label(connection_frame, text="COM Port:").pack(side="left", padx=5, pady=5)

        self.port_var = tk.StringVar()
        self.port_menu = ttk.Combobox(connection_frame, textvariable=self.port_var, state="readonly", width=40)
        self.port_menu.pack(side="left", padx=5, pady=5)

        self.refresh_button = ttk.Button(connection_frame, text="Refresh", command=self.populate_ports)
        self.refresh_button.pack(side="left", padx=5, pady=5)

        self.connect_button = ttk.Button(connection_frame, text="Connect", command=self.connect_serial)
        self.connect_button.pack(side="left", padx=5, pady=5)

        self.disconnect_button = ttk.Button(connection_frame, text="Disconnect", command=self.disconnect_serial, state="disabled")
        self.disconnect_button.pack(side="left", padx=5, pady=5)

        # --- Middle Frame for Actions ---
        action_frame = ttk.LabelFrame(self, text="Actions")
        action_frame.pack(padx=10, pady=5, fill="x")

        self.capture_button = ttk.Button(action_frame, text="Capture ('c')", command=self.send_capture_command, state="disabled")
        self.capture_button.pack(side="left", padx=10, pady=10)

        self.transmit_button = ttk.Button(action_frame, text="Transmit ('t')", command=self.send_transmit_command, state="disabled")
        self.transmit_button.pack(side="left", padx=10, pady=10)

        # --- Bottom Frame for Log ---
        log_frame = ttk.LabelFrame(self, text="Log")
        log_frame.pack(padx=10, pady=10, expand=True, fill="both")

        self.log_text = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, state="disabled", font=("Courier New", 9))
        self.log_text.pack(expand=True, fill="both")

        self.populate_ports()
        self.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.after(100, self.process_queue)

    def populate_ports(self):
        """Scans for serial ports and populates the dropdown menu."""
        ports = serial.tools.list_ports.comports()
        # We want to display a user-friendly list, but use the device path for connection
        self.port_map = {f"{p.device} - {p.description}": p.device for p in ports}
        self.port_menu['values'] = list(self.port_map.keys())
        if self.port_map:
            self.port_menu.current(0)
        else:
            self.port_var.set("No ports found")

    def connect_serial(self):
        """Establishes the serial connection."""
        selected_display_name = self.port_var.get()
        if not selected_display_name or "No ports found" in selected_display_name:
            messagebox.showerror("Error", "No serial port selected.")
            return

        port = self.port_map.get(selected_display_name)
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            self.log_message(f"SYSTEM > Successfully connected to {port}\n")

            self.update_gui_state("connected")

            # Start a thread to read from the serial port
            self.stop_thread = False
            self.port_thread = threading.Thread(target=self.read_from_port)
            self.port_thread.daemon = True
            self.port_thread.start()

        except serial.SerialException as e:
            messagebox.showerror("Connection Failed", f"Failed to connect to {port}.\nError: {e}")
            self.serial_port = None

    def disconnect_serial(self):
        """Closes the serial connection."""
        if self.serial_port and self.serial_port.is_open:
            self.stop_thread = True
            if self.port_thread:
                self.port_thread.join(timeout=2)
            self.serial_port.close()
            self.log_message(f"SYSTEM > Disconnected from serial port.\n")
            self.serial_port = None

        self.update_gui_state("disconnected")

    def update_gui_state(self, state):
        """Updates the GUI widgets based on connection state."""
        if state == "connected":
            self.connect_button.config(state="disabled")
            self.disconnect_button.config(state="normal")
            self.port_menu.config(state="disabled")
            self.refresh_button.config(state="disabled")
            self.capture_button.config(state="normal")
            self.transmit_button.config(state="normal")
        elif state == "disconnected":
            self.connect_button.config(state="normal")
            self.disconnect_button.config(state="disabled")
            self.port_menu.config(state="readonly")
            self.refresh_button.config(state="normal")
            self.capture_button.config(state="disabled")
            self.transmit_button.config(state="disabled")

    def read_from_port(self):
        """Runs in a separate thread to read data from the serial port."""
        while not self.stop_thread and self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line:
                    self.data_queue.put(line)
            except (serial.SerialException, TypeError):
                self.data_queue.put("SYSTEM > ERROR: Serial port has been disconnected.")
                break

    def process_queue(self):
        """Processes messages from the serial thread queue to update the GUI."""
        try:
            while True:
                line = self.data_queue.get_nowait()
                self.log_message(f"PICO > {line}\n")
                if "ERROR:" in line:
                    self.disconnect_serial()
        except queue.Empty:
            pass # No new messages
        self.after(100, self.process_queue)

    def send_command(self, command: str):
        """Sends a command to the Pico."""
        if self.serial_port and self.serial_port.is_open:
            self.log_message(f"PC > Sending command: '{command}'\n")
            self.serial_port.write(command.encode('utf-8'))
        else:
            messagebox.showwarning("Warning", "Not connected to a serial port.")

    def send_capture_command(self):
        self.send_command('c')

    def send_transmit_command(self):
        self.send_command('t')

    def log_message(self, message: str):
        """Appends a message to the log text area in a thread-safe way."""
        self.log_text.config(state="normal")
        self.log_text.insert(tk.END, message)
        self.log_text.see(tk.END)
        self.log_text.config(state="disabled")

    def on_closing(self):
        """Handles the window closing event."""
        if self.serial_port and self.serial_port.is_open:
            self.disconnect_serial()
        self.destroy()

if __name__ == "__main__":
    # Inform user about dependencies if they are not installed
    try:
        import serial
    except ImportError:
        messagebox.showerror("Dependency Missing",
                             "The 'pyserial' library is not installed.\n"
                             "Please install it by running:\n\n"
                             "pip install pyserial")
        exit()

    app = App()
    app.mainloop()
