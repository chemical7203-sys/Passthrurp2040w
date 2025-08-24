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

        # Expanded state for v2 protocol
        self.serial_state = {
            'buttons': 0, 'dpad': 0,
            'lx': 0, 'ly': 0, 'rx': 0, 'ry': 0,
            'l2': 0, 'r2': 0,
        }
        # Map pygame button index to bitmask
        self.button_map = {0:1, 1:2, 2:4, 3:8, 4:16, 5:32, 8:64, 9:128, 10:256, 11:512}

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
        self.gamepad_signals.dpad_event.connect(self.ui.gamepad_widget.update_dpad)
        self.gamepad_signals.trigger_event.connect(self.ui.gamepad_widget.update_trigger)
        self.gamepad_signals.gamepad_disconnected.connect(self.handle_gamepad_disconnect)
        self.gamepad_signals.raw_event.connect(self.ui.log_raw_event)

        # Connect all events to update the serial state
        self.gamepad_signals.stick_event.connect(self.update_serial_state)
        self.gamepad_signals.button_event.connect(self.update_serial_state)
        self.gamepad_signals.trigger_event.connect(self.update_serial_state)
        self.gamepad_signals.dpad_event.connect(self.update_serial_state)

    def refresh_all_devices(self):
        self.refresh_serial_ports()
        self.refresh_gamepads()

    def refresh_serial_ports(self):
        self.ui.serial_select.clear()
        ports = get_available_serial_ports()
        self.ui.serial_select.addItem("Select a port...", None)
        for port in ports:
            self.ui.serial_select.addItem(f"{port.device} - {port.description}", port.device)

    def refresh_gamepads(self):
        self.ui.gamepad_select.clear()
        self.gamepads = get_available_gamepads()
        self.ui.gamepad_select.addItem("Select a gamepad...", -1)
        for gamepad in self.gamepads:
            self.ui.gamepad_select.addItem(gamepad['name'], gamepad['index'])

    def select_gamepad(self, index):
        joystick_index = self.ui.gamepad_select.itemData(index)
        self.ds4_handler.set_device(joystick_index if joystick_index >= 0 else None)

    def handle_gamepad_disconnect(self):
        QMessageBox.warning(self.ui, "Gamepad Disconnected", "The connection to the gamepad was lost.")
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
                QMessageBox.critical(self.ui, "Connection Error", f"Failed to connect to port {port}.")

    def update_serial_state(self, code, value):
        # Sticks
        if code == 'ABS_X': self.serial_state['lx'] = int(value * 127)
        elif code == 'ABS_Y': self.serial_state['ly'] = int(value * 127)
        elif code == 'ABS_RX': self.serial_state['rx'] = int(value * 127)
        elif code == 'ABS_RY': self.serial_state['ry'] = int(value * 127)
        # Triggers
        elif code == 'ABS_Z': self.serial_state['l2'] = int((value + 1) / 2 * 255)
        elif code == 'ABS_RZ': self.serial_state['r2'] = int((value + 1) / 2 * 255)
        # Buttons
        elif code in self.button_map:
            bit = self.button_map[code]
            if value: # pressed
                self.serial_state['buttons'] |= bit
            else: # released
                self.serial_state['buttons'] &= ~bit
        # D-Pad
        elif code == 'ABS_HAT0X':
            self.serial_state['dpad'] &= ~0b0100 # Clear left
            self.serial_state['dpad'] &= ~0b1000 # Clear right
            if value < 0: self.serial_state['dpad'] |= 0b0100 # Set left
            elif value > 0: self.serial_state['dpad'] |= 0b1000 # Set right
        elif code == 'ABS_HAT0Y':
            self.serial_state['dpad'] &= ~0b0001 # Clear up
            self.serial_state['dpad'] &= ~0b0010 # Clear down
            if value < 0: self.serial_state['dpad'] |= 0b0001 # Set up
            elif value > 0: self.serial_state['dpad'] |= 0b0010 # Set down

    def send_latest_serial_state(self):
        self.serial_handler.send_gamepad_state_v2(self.serial_state)

    def run(self):
        self.ui.show()
        self.ds4_handler.start()
        self.app.aboutToQuit.connect(self.cleanup)
        sys.exit(self.app.exec())

    def cleanup(self):
        print("Cleaning up...")
        self.serial_timer.stop()
        self.ds4_handler.stop()
        self.serial_handler.disconnect()

if __name__ == '__main__':
    main_app = MainApplication()
    main_app.run()
