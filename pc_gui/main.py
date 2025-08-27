import sys
from PyQt6.QtWidgets import QApplication, QMessageBox
from PyQt6.QtCore import QTimer
from gamepad_ui import GamepadUI, GamepadSignals
from ds4_handler import DS4Handler
from serial_handler import SerialHandler
from device_manager import get_available_gamepads, get_available_serial_ports
from queue import Queue

class MainApplication:
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.ui = GamepadUI()
        self.gamepad_signals = GamepadSignals()

        self.command_queue = Queue()
        self.ds4_handler = DS4Handler(self.gamepad_signals, self.command_queue)
        self.serial_handler = SerialHandler()

        self.serial_state = {
            'buttons': 0, 'dpad': 0,
            'lx': 0, 'ly': 0, 'rx': 0, 'ry': 0,
            'l2': 0, 'r2': 0,
        }
        # This mapping seems to be for the v2 serial protocol, not directly for DS4 buttons.
        # Let's assume the button mapping from ds4_handler is the source of truth for the UI
        # and this mapping is for serializing the state.
        self.button_map = {
            'BTN_SOUTH':  ( 'buttons', 1<<0 ), # Cross
            'BTN_EAST':   ( 'buttons', 1<<1 ), # Circle
            'BTN_WEST':   ( 'buttons', 1<<2 ), # Square
            'BTN_NORTH':  ( 'buttons', 1<<3 ), # Triangle
            'BTN_TL':     ( 'buttons', 1<<4 ), # L1
            'BTN_TR':     ( 'buttons', 1<<5 ), # R1
            # L2/R2 are analog, handled separately. These are for the button press.
            'BTN_TL2':    ( 'buttons', 1<<6 ), # L2 Press
            'BTN_TR2':    ( 'buttons', 1<<7 ), # R2 Press
            'BTN_SELECT': ( 'buttons', 1<<8 ), # Share
            'BTN_START':  ( 'buttons', 1<<9 ), # Options
            'BTN_THUMBL': ( 'buttons', 1<<10 ), # L3
            'BTN_THUMBR': ( 'buttons', 1<<11 ), # R3
            'BTN_MODE':   ( 'buttons', 1<<12), # PS Button
            'DPAD_UP':    ( 'dpad', 1<<0 ),
            'DPAD_DOWN':  ( 'dpad', 1<<1 ),
            'DPAD_LEFT':  ( 'dpad', 1<<2 ),
            'DPAD_RIGHT': ( 'dpad', 1<<3 ),
        }

        self.gamepads = []
        self.connect_signals()
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
        self.ui.serial_select.clear()
        ports = get_available_serial_ports()
        self.ui.serial_select.addItem("Select a port...", None)
        for port in ports: self.ui.serial_select.addItem(f"{port.device}", port.device)

    def refresh_gamepads(self):
        print("DEBUG: Main: Refreshing gamepads...")
        current_selection = self.ui.gamepad_select.currentData()
        self.ui.gamepad_select.blockSignals(True)
        self.ui.gamepad_select.clear()
        self.gamepads = get_available_gamepads()
        self.ui.gamepad_select.addItem("Select a gamepad...", None)
        found_current = False
        for i, gamepad in enumerate(self.gamepads):
            self.ui.gamepad_select.addItem(gamepad['name'], gamepad['index'])
            if gamepad['index'] == current_selection:
                self.ui.gamepad_select.setCurrentIndex(i + 1)
                found_current = True

        if not found_current:
            self.command_queue.put({'type': 'SET_DEVICE', 'index': None})

        self.ui.gamepad_select.blockSignals(False)
        print(f"DEBUG: Main: Gamepads refreshed. Count: {len(self.gamepads)}")

    def select_gamepad(self, index):
        if index < 0: return # Should not happen, but as a safeguard
        joystick_index = self.ui.gamepad_select.itemData(index)
        command = {'type': 'SET_DEVICE', 'index': joystick_index}
        print(f"DEBUG: Main: Putting command to queue: {command}")
        self.command_queue.put(command)

    def handle_gamepad_disconnect(self):
        print("DEBUG: Main: Gamepad disconnected signal received.")
        QMessageBox.warning(self.ui, "Gamepad Disconnected", "Connection to the current gamepad was lost.")
        self.refresh_gamepads()

    def toggle_serial_connection(self):
        if self.serial_handler.ser and self.serial_handler.ser.is_open:
            self.serial_handler.disconnect()
            self.ui.serial_connect_btn.setText("Connect")
        else:
            port = self.ui.serial_select.currentData()
            if port and self.serial_handler.connect(port):
                self.ui.serial_connect_btn.setText("Disconnect")
            elif port:
                QMessageBox.critical(self.ui, "Connection Error", f"Failed to connect to {port}.")

    def update_serial_state(self, code, value):
        # This mapping is for the v2 serial protocol sent TO the Pico
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
        self.ds4_handler.start()
        self.app.aboutToQuit.connect(self.cleanup)
        sys.exit(self.app.exec())

    def cleanup(self):
        print("DEBUG: Main: Cleanup called. Sending STOP command.")
        self.tx_timer.stop()
        self.rx_monitor_timer.stop()
        self.command_queue.put({'type': 'STOP'})
        self.ds4_handler.join() # Wait for the thread to finish
        self.serial_handler.disconnect()

if __name__ == '__main__':
    main_app = MainApplication()
    main_app.run()
