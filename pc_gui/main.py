import sys
from PyQt6.QtWidgets import QApplication, QMessageBox
from PyQt6.QtCore import QTimer
from gamepad_ui import GamepadUI, GamepadSignals
from ds4_handler import DS4Handler
from serial_handler import SerialHandler
from device_manager import get_available_gamepads, get_available_serial_ports

class MainApplication:
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.ui = GamepadUI()
        self.gamepad_signals = GamepadSignals()
        self.ds4_handler = DS4Handler(self.gamepad_signals)
        self.serial_handler = SerialHandler()

        self.serial_state = {
            'buttons': 0, 'dpad': 0,
            'lx': 0, 'ly': 0, 'rx': 0, 'ry': 0,
            'l2': 0, 'r2': 0,
        }
        # Combined map for all buttons including D-Pad
        self.button_map = {
            'BTN_SOUTH':  ( 'buttons', 0b1 ),
            'BTN_EAST':   ( 'buttons', 0b10 ),
            'BTN_WEST':   ( 'buttons', 0b100 ),
            'BTN_NORTH':  ( 'buttons', 0b1000 ),
            'BTN_TL':     ( 'buttons', 0b10000 ),
            'BTN_TR':     ( 'buttons', 0b100000 ),
            'BTN_THUMBL': ( 'buttons', 0b10000000 ),
            'BTN_THUMBR': ( 'buttons', 0b100000000 ),
            'DPAD_UP':    ( 'dpad', 0b1 ),
            'DPAD_DOWN':  ( 'dpad', 0b10 ),
            'DPAD_LEFT':  ( 'dpad', 0b100 ),
            'DPAD_RIGHT': ( 'dpad', 0b1000 ),
        }

        self.gamepads = []
        self.connect_signals()
        self.refresh_all_devices()
        self.serial_timer = QTimer()
        self.serial_timer.timeout.connect(self.send_latest_serial_state)
        self.serial_timer.start(8)

    def connect_signals(self):
        self.ui.serial_connect_btn.clicked.connect(self.toggle_serial_connection)
        self.ui.serial_refresh_btn.clicked.connect(self.refresh_serial_ports)
        self.ui.gamepad_refresh_btn.clicked.connect(self.refresh_gamepads)
        self.ui.gamepad_select.currentIndexChanged.connect(self.select_gamepad)

        self.gamepad_signals.stick_event.connect(self.ui.gamepad_widget.update_stick)
        self.gamepad_signals.button_event.connect(self.ui.gamepad_widget.update_button)
        self.gamepad_signals.trigger_event.connect(self.ui.gamepad_widget.update_trigger)
        self.gamepad_signals.gamepad_disconnected.connect(self.handle_gamepad_disconnect)
        self.gamepad_signals.raw_event.connect(self.ui.log_raw_event)

        self.gamepad_signals.stick_event.connect(self.update_serial_state)
        self.gamepad_signals.button_event.connect(self.update_serial_state)
        self.gamepad_signals.trigger_event.connect(self.update_serial_state)

    def refresh_all_devices(self):
        self.refresh_serial_ports(); self.refresh_gamepads()

    def refresh_serial_ports(self):
        self.ui.serial_select.clear(); ports = get_available_serial_ports()
        self.ui.serial_select.addItem("Select a port...", None)
        for port in ports: self.ui.serial_select.addItem(f"{port.device}", port.device)

    def refresh_gamepads(self):
        self.ui.gamepad_select.clear(); self.gamepads = get_available_gamepads()
        self.ui.gamepad_select.addItem("Select a gamepad...", -1)
        for gamepad in self.gamepads: self.ui.gamepad_select.addItem(gamepad['name'], gamepad['index'])

    def select_gamepad(self, index):
        joystick_index = self.ui.gamepad_select.itemData(index)
        self.ds4_handler.set_device(joystick_index if joystick_index >= 0 else None)

    def handle_gamepad_disconnect(self):
        QMessageBox.warning(self.ui, "Gamepad Disconnected", "Connection lost.")
        self.refresh_gamepads()

    def toggle_serial_connection(self):
        if self.serial_handler.ser and self.serial_handler.ser.is_open:
            self.serial_handler.disconnect(); self.ui.serial_connect_btn.setText("Connect")
        else:
            port = self.ui.serial_select.currentData()
            if port and self.serial_handler.connect(port): self.ui.serial_connect_btn.setText("Disconnect")
            elif port: QMessageBox.critical(self.ui, "Connection Error", f"Failed to connect to {port}.")

    def update_serial_state(self, code, value):
        # Sticks and Triggers
        if code == 'ABS_X': self.serial_state['lx'] = int(value * 127)
        elif code == 'ABS_Y': self.serial_state['ly'] = int(value * 127)
        elif code == 'ABS_RX': self.serial_state['rx'] = int(value * 127)
        elif code == 'ABS_RY': self.serial_state['ry'] = int(value * 127)
        elif code == 'ABS_Z': self.serial_state['l2'] = int((value + 1) / 2 * 255)
        elif code == 'ABS_RZ': self.serial_state['r2'] = int((value + 1) / 2 * 255)
        # Buttons (including D-Pad as buttons)
        elif code in self.button_map:
            state_key, bit = self.button_map[code]
            if value: self.serial_state[state_key] |= bit
            else: self.serial_state[state_key] &= ~bit

    def send_latest_serial_state(self):
        self.serial_handler.send_gamepad_state_v2(self.serial_state)

    def run(self):
        self.ui.show(); self.ds4_handler.start(); self.app.aboutToQuit.connect(self.cleanup); sys.exit(self.app.exec())

    def cleanup(self):
        self.serial_timer.stop(); self.ds4_handler.stop(); self.serial_handler.disconnect()

if __name__ == '__main__':
    main_app = MainApplication()
    main_app.run()
