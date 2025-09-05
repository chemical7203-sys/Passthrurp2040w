import sys
from PyQt6.QtWidgets import QApplication, QMessageBox
from PyQt6.QtCore import QTimer
from gamepad_ui import GamepadUI, GamepadSignals
from ds4_handler import DS4Handler
from serial_handler import SerialHandler
from queue import Queue
import serial.tools.list_ports

class MainApplication:
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.ui = GamepadUI()
        self.gamepad_signals = GamepadSignals()

        # This dictionary will be shared between the ds4_handler and this main thread
        self.serial_state = {
            'buttons': 0, 'dpad': 0, 'lx': 0, 'ly': 0, 'rx': 0, 'ry': 0, 'l2': 0, 'r2': 0,
            'accel_x': 0, 'accel_y': 0, 'accel_z': 0,
            'gyro_x': 0, 'gyro_y': 0, 'gyro_z': 0,
        }

        self.command_queue = Queue()
        self.ds4_handler = DS4Handler(self.gamepad_signals, self.command_queue, self.serial_state)
        self.serial_handler = SerialHandler()

        self.connect_signals()
        self.ds4_handler.start()
        self.refresh_all_devices()

        # Timer to send data to the Pico
        self.tx_timer = QTimer()
        self.tx_timer.timeout.connect(self.send_latest_serial_state)
        self.tx_timer.start(5) # Send data every 5ms for 200Hz, good for motion controls

        # Timer to update the UI
        self.ui_timer = QTimer()
        self.ui_timer.timeout.connect(self.update_ui_from_state)
        self.ui_timer.start(33) # Update UI at ~30fps

        # Timer to read debug messages from the Pico
        self.rx_monitor_timer = QTimer()
        self.rx_monitor_timer.timeout.connect(self.update_rx_monitor)
        self.rx_monitor_timer.start(100)

    def connect_signals(self):
        self.ui.serial_connect_btn.clicked.connect(self.toggle_serial_connection)
        self.ui.serial_refresh_btn.clicked.connect(self.refresh_serial_ports)
        # The gamepad selection UI is now removed, so we don't need these connections
        # self.ui.gamepad_refresh_btn.clicked.connect(self.refresh_gamepads)
        # self.ui.gamepad_select.currentIndexChanged.connect(self.select_gamepad)

        # We also don't connect the old signals, as the state is shared directly
        self.gamepad_signals.gamepad_list_updated.connect(self.on_gamepad_list_updated)


    def update_ui_from_state(self):
        """Updates the UI widgets directly from the shared serial_state."""
        # Sticks
        self.ui.gamepad_widget.update_stick('ABS_X', self.serial_state['lx'] / 128.0)
        self.ui.gamepad_widget.update_stick('ABS_Y', self.serial_state['ly'] / 128.0)
        self.ui.gamepad_widget.update_stick('ABS_RX', self.serial_state['rx'] / 128.0)
        self.ui.gamepad_widget.update_stick('ABS_RY', self.serial_state['ry'] / 128.0)
        # Triggers
        self.ui.gamepad_widget.update_trigger('ABS_Z', (self.serial_state['l2'] / 255.0) * 2 - 1)
        self.ui.gamepad_widget.update_trigger('ABS_RZ', (self.serial_state['r2'] / 255.0) * 2 - 1)
        # Gyro/Accel for logging/display
        self.ui.log_raw_event(
            f"A: {self.serial_state['accel_x']:6d}, {self.serial_state['accel_y']:6d}, {self.serial_state['accel_z']:6d} | "
            f"G: {self.serial_state['gyro_x']:6d}, {self.serial_state['gyro_y']:6d}, {self.serial_state['gyro_z']:6d}"
        )

    def update_rx_monitor(self):
        lines = self.serial_handler.get_all_received_lines()
        if lines:
            self.ui.log_uart_rx('\n'.join(lines))

    def refresh_all_devices(self):
        self.refresh_serial_ports()
        # No need to refresh gamepads from here, handler does it automatically
        # self.refresh_gamepads()

    def refresh_serial_ports(self):
        ports = serial.tools.list_ports.comports()
        self.ui.serial_select.clear()
        self.ui.serial_select.addItem("Select a port...", None)
        for port in ports: self.ui.serial_select.addItem(f"{port.device}", port.device)

    def on_gamepad_list_updated(self, gamepads):
        """Receives the new gamepad list from the handler and updates the UI."""
        # We can just update a label to show connection status
        if gamepads:
            self.ui.set_gamepad_status(f"Connected: {gamepads[0]['name']}")
        else:
            self.ui.set_gamepad_status("Disconnected")

    def toggle_serial_connection(self):
        if self.serial_handler.ser and self.serial_handler.ser.is_open:
            self.serial_handler.disconnect(); self.ui.serial_connect_btn.setText("Connect")
        else:
            port = self.ui.serial_select.currentData()
            if port and self.serial_handler.connect(port): self.ui.serial_connect_btn.setText("Disconnect")
            elif port: QMessageBox.critical(self.ui, "Connection Error", f"Failed to connect to {port}.")

    def send_latest_serial_state(self):
        if self.serial_handler.ser and self.serial_handler.ser.is_open:
            # Send the new V3 packet
            self.serial_handler.send_gamepad_state_v3(self.serial_state)

    def run(self):
        self.ui.show()
        self.app.aboutToQuit.connect(self.cleanup)
        sys.exit(self.app.exec())

    def cleanup(self):
        self.tx_timer.stop()
        self.ui_timer.stop()
        self.rx_monitor_timer.stop()
        self.command_queue.put({'type': 'STOP'})
        self.ds4_handler.join()
        self.serial_handler.disconnect()

if __name__ == '__main__':
    main_app = MainApplication()
    main_app.run()
