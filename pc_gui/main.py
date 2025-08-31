import sys
from PyQt6.QtWidgets import QApplication, QMessageBox
from PyQt6.QtCore import QTimer
from gamepad_ui import GamepadUI, GamepadSignals
from gamepad_handler import GamepadHandler
from serial_handler import SerialHandler
from queue import Queue

class MainApplication:
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.ui = GamepadUI()
        self.gamepad_signals = GamepadSignals()

        self.command_queue = Queue()
        self.gamepad_handler = GamepadHandler(self.gamepad_signals, self.command_queue)
        self.serial_handler = SerialHandler()

        self.serial_state = {
            'buttons': 0, 'dpad': 0, 'lx': 0, 'ly': 0, 'rx': 0, 'ry': 0, 'l2': 0, 'r2': 0,
        }
        self.button_map = {
            'BTN_SOUTH': ('buttons', 1<<0), 'BTN_EAST': ('buttons', 1<<1),
            'BTN_WEST': ('buttons', 1<<2), 'BTN_NORTH': ('buttons', 1<<3),
            'BTN_TL': ('buttons', 1<<4), 'BTN_TR': ('buttons', 1<<5),
            'BTN_TL2': ('buttons', 1<<6), 'BTN_TR2': ('buttons', 1<<7),
            'BTN_SELECT': ('buttons', 1<<8), 'BTN_START': ('buttons', 1<<9),
            'BTN_THUMBL': ('buttons', 1<<10), 'BTN_THUMBR': ('buttons', 1<<11),
            'BTN_MODE': ('buttons', 1<<12),
            'DPAD_UP': ('dpad', 1<<0), 'DPAD_DOWN': ('dpad', 1<<1),
            'DPAD_LEFT': ('dpad', 1<<2), 'DPAD_RIGHT': ('dpad', 1<<3),
        }

        self.connect_signals()
        self.gamepad_handler.start()
        self.refresh_all_devices()

        self.tx_timer = QTimer()
        self.tx_timer.timeout.connect(self.send_latest_serial_state)
        self.tx_timer.start(8)

        self.rx_monitor_timer = QTimer()
        self.rx_monitor_timer.timeout.connect(self.update_rx_monitor)
        self.rx_monitor_timer.start(200)

    def connect_signals(self):
        self.ui.serial_connect_btn.clicked.connect(self.toggle_serial_connection)
        self.ui.serial_refresh_btn.clicked.connect(self.refresh_serial_ports)
        self.ui.gamepad_refresh_btn.clicked.connect(self.refresh_gamepads)
        self.ui.gamepad_select.currentIndexChanged.connect(self.select_gamepad)

        self.gamepad_signals.stick_event.connect(self.ui.gamepad_widget.update_stick)
        self.gamepad_signals.button_event.connect(self.ui.gamepad_widget.update_button)
        self.gamepad_signals.trigger_event.connect(self.ui.gamepad_widget.update_trigger)
        self.gamepad_signals.gamepad_disconnected.connect(self.handle_gamepad_disconnect)
        self.gamepad_signals.device_changed.connect(self.refresh_gamepads)
        self.gamepad_signals.gamepad_list_updated.connect(self.on_gamepad_list_updated)
        self.gamepad_signals.raw_event.connect(self.ui.log_raw_event)

        self.gamepad_signals.stick_event.connect(self.update_serial_state)
        self.gamepad_signals.button_event.connect(self.update_serial_state)
        self.gamepad_signals.trigger_event.connect(self.update_serial_state)

    def update_rx_monitor(self):
        lines = self.serial_handler.get_all_received_lines()
        if lines:
            self.ui.log_uart_rx('\n'.join(lines))

    def refresh_all_devices(self):
        self.refresh_serial_ports()
        self.refresh_gamepads()

    def refresh_serial_ports(self):
        # This function does not interact with Pygame, so it's safe.
        from device_manager import get_available_serial_ports
        self.ui.serial_select.clear()
        ports = get_available_serial_ports()
        self.ui.serial_select.addItem("Select a port...", None)
        for port in ports: self.ui.serial_select.addItem(f"{port.device}", port.device)

    def refresh_gamepads(self):
        """Sends a command to the handler thread to refresh the device list."""
        self.command_queue.put({'type': 'REFRESH_DEVICES'})

    def on_gamepad_list_updated(self, gamepads):
        """Receives the new gamepad list from the handler and updates the UI."""
        current_selection = self.ui.gamepad_select.currentData()
        self.ui.gamepad_select.blockSignals(True)
        self.ui.gamepad_select.clear()
        self.ui.gamepad_select.addItem("Select a gamepad...", None)

        found_current = False
        for i, gamepad in enumerate(gamepads):
            self.ui.gamepad_select.addItem(gamepad['name'], gamepad['index'])
            if gamepad['index'] == current_selection:
                self.ui.gamepad_select.setCurrentIndex(i + 1)
                found_current = True

        if not found_current and current_selection is not None:
             self.command_queue.put({'type': 'SET_DEVICE', 'index': None})

        self.ui.gamepad_select.blockSignals(False)

    def select_gamepad(self, index):
        if index < 0: return
        joystick_index = self.ui.gamepad_select.itemData(index)
        command = {'type': 'SET_DEVICE', 'index': joystick_index}
        self.command_queue.put(command)

    def handle_gamepad_disconnect(self):
        QMessageBox.warning(self.ui, "Gamepad Disconnected", "Connection to the current gamepad was lost.")
        self.refresh_gamepads()

    def toggle_serial_connection(self):
        if self.serial_handler.ser and self.serial_handler.ser.is_open:
            self.serial_handler.disconnect(); self.ui.serial_connect_btn.setText("Connect")
        else:
            port = self.ui.serial_select.currentData()
            if port and self.serial_handler.connect(port): self.ui.serial_connect_btn.setText("Disconnect")
            elif port: QMessageBox.critical(self.ui, "Connection Error", f"Failed to connect to {port}.")

    def update_serial_state(self, code, value):
        if code == 'ABS_X': self.serial_state['lx'] = int(value * 127)
        elif code == 'ABS_Y': self.serial_state['ly'] = int(value * 127)
        elif code == 'ABS_RX': self.serial_state['rx'] = int(value * 127)
        elif code == 'ABS_RY': self.serial_state['ry'] = int(value * 127)
        elif code == 'ABS_Z': self.serial_state['l2'] = int((value + 1) / 2 * 255)
        elif code == 'ABS_RZ': self.serial_state['r2'] = int((value + 1) / 2 * 255)
        elif code in self.button_map:
            state_key, bit = self.button_map[code]
            if value: self.serial_state[state_key] |= bit
            else: self.serial_state[state_key] &= ~bit

    def send_latest_serial_state(self):
        if self.serial_handler.ser and self.serial_handler.ser.is_open:
            self.serial_handler.send_gamepad_state_v2(self.serial_state)

    def run(self):
        self.ui.show()
        self.app.aboutToQuit.connect(self.cleanup)
        sys.exit(self.app.exec())

    def cleanup(self):
        self.tx_timer.stop()
        self.rx_monitor_timer.stop()
        self.command_queue.put({'type': 'STOP'})
        self.gamepad_handler.join()
        self.serial_handler.disconnect()

if __name__ == '__main__':
    main_app = MainApplication()
    main_app.run()
