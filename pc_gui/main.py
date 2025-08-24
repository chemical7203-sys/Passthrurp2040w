import sys
from PyQt6.QtWidgets import QApplication, QMessageBox
from PyQt6.QtCore import QTimer
from gamepad_ui import GamepadUI, GamepadSignals
from ds4_handler import DS4Handler
from serial_handler import SerialHandler
from device_manager import get_available_gamepads, get_available_serial_ports

class MainApplication:
    """The main class that orchestrates the UI, device discovery, and handlers."""
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.ui = GamepadUI()
        self.gamepad_signals = GamepadSignals()
        self.ds4_handler = DS4Handler(self.gamepad_signals)
        self.serial_handler = SerialHandler()
        self.last_serial_state = {'buttons': 0, 'x': 0, 'y': 0}
        self.gamepads = []
        self.connect_signals()
        self.refresh_all_devices()
        self.serial_timer = QTimer()
        self.serial_timer.timeout.connect(self.send_latest_serial_state)
        self.serial_timer.start(8)

    def connect_signals(self):
        """Connects signals from the UI and gamepad handler to the appropriate slots."""
        # UI signals
        self.ui.serial_connect_btn.clicked.connect(self.toggle_serial_connection)
        self.ui.serial_refresh_btn.clicked.connect(self.refresh_serial_ports)
        self.ui.gamepad_refresh_btn.clicked.connect(self.refresh_gamepads)
        self.ui.gamepad_select.currentIndexChanged.connect(self.select_gamepad)

        # Gamepad signals for UI updates (Corrected to point to gamepad_widget)
        self.gamepad_signals.stick_event.connect(self.ui.gamepad_widget.update_stick)
        self.gamepad_signals.button_event.connect(self.ui.gamepad_widget.update_button)
        self.gamepad_signals.gamepad_disconnected.connect(self.handle_gamepad_disconnect)

        # Gamepad signals for updating the state to be sent
        self.gamepad_signals.stick_event.connect(self.update_serial_state)
        self.gamepad_signals.button_event.connect(self.update_serial_state)

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

    def update_serial_state(self, event_code, event_value):
        """This slot only updates the state dictionary. It does not send data."""
        # This mapping is simplified for the v1 protocol
        if event_code == 'BTN_SOUTH':
            self.last_serial_state['buttons'] = 1 if event_value else 0
        elif event_code == 'ABS_X':
            # Pygame axis is -1.0 to 1.0. Convert to -127 to 127
            self.last_serial_state['x'] = int(event_value * 127)
        elif event_code == 'ABS_Y':
            self.last_serial_state['y'] = int(event_value * 127)

    def send_latest_serial_state(self):
        """Called by the QTimer to send the most recent state."""
        self.serial_handler.send_gamepad_state(
            self.last_serial_state['buttons'],
            self.last_serial_state['x'],
            self.last_serial_state['y']
        )

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
