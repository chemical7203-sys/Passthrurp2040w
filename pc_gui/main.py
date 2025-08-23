import sys
from PyQt6.QtWidgets import QApplication, QMessageBox
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

        # Start with a stopped handler; it will be configured and started later
        self.ds4_handler = DS4Handler(self.gamepad_signals)

        self.serial_handler = SerialHandler()
        self.last_serial_state = {'buttons': 0, 'x': 0, 'y': 0}

        self.gamepads = [] # To store the list of found gamepads

        self.connect_signals()
        self.refresh_all_devices()

    def connect_signals(self):
        """Connects signals from the UI and gamepad handler to the appropriate slots."""
        # UI signals
        self.ui.serial_connect_btn.clicked.connect(self.toggle_serial_connection)
        self.ui.serial_refresh_btn.clicked.connect(self.refresh_serial_ports)
        self.ui.gamepad_refresh_btn.clicked.connect(self.refresh_gamepads)
        self.ui.gamepad_select.currentIndexChanged.connect(self.select_gamepad)

        # Gamepad signals for UI updates are now connected to the new gamepad_widget
        self.gamepad_signals.stick_event.connect(self.ui.gamepad_widget.update_stick)
        self.gamepad_signals.button_event.connect(self.ui.gamepad_widget.update_button)
        # Note: trigger and dpad are not visualized in the new widget yet, so we leave them disconnected.
        self.gamepad_signals.gamepad_disconnected.connect(self.handle_gamepad_disconnect)

        # Connect gamepad events to the serial sender
        self.gamepad_signals.stick_event.connect(self.handle_gamepad_event_for_serial)
        self.gamepad_signals.button_event.connect(self.handle_gamepad_event_for_serial)

    def refresh_all_devices(self):
        self.refresh_serial_ports()
        self.refresh_gamepads()

    def refresh_serial_ports(self):
        self.ui.serial_select.clear()
        ports = get_available_serial_ports()
        for port in ports:
            self.ui.serial_select.addItem(f"{port.device} - {port.description}", port.device)

    def refresh_gamepads(self):
        self.ui.gamepad_select.clear()
        self.gamepads = get_available_gamepads()
        self.ui.gamepad_select.addItem("Select a gamepad...", None)
        for i, gamepad in enumerate(self.gamepads):
            self.ui.gamepad_select.addItem(gamepad.name, gamepad.path)

    def select_gamepad(self, index):
        """Starts listening to the selected gamepad."""
        if index <= 0: # "Select a gamepad..."
            self.ds4_handler.set_device(None)
            return

        device_path = self.ui.gamepad_select.itemData(index)
        self.ds4_handler.set_device(device_path)

    def handle_gamepad_disconnect(self):
        QMessageBox.warning(self.ui, "Gamepad Disconnected", "The connection to the gamepad was lost.")
        self.refresh_gamepads()

    def toggle_serial_connection(self):
        """Connects or disconnects the serial port."""
        if self.serial_handler.ser and self.serial_handler.ser.is_open:
            self.serial_handler.disconnect()
            self.ui.serial_connect_btn.setText("Connect")
        else:
            port = self.ui.serial_select.currentData()
            if port and self.serial_handler.connect(port):
                self.ui.serial_connect_btn.setText("Disconnect")
            elif port:
                QMessageBox.critical(self.ui, "Connection Error", f"Failed to connect to port {port}.")

    def handle_gamepad_event_for_serial(self, event_code, event_value):
        """Processes gamepad events and sends them over serial."""
        if event_code == 'BTN_SOUTH':
            self.last_serial_state['buttons'] = 1 if event_value else 0
        elif event_code == 'ABS_X':
            self.last_serial_state['x'] = event_value - 128
        elif event_code == 'ABS_Y':
            self.last_serial_state['y'] = event_value - 128

        self.serial_handler.send_gamepad_state(
            self.last_serial_state['buttons'],
            self.last_serial_state['x'],
            self.last_serial_state['y']
        )

    def run(self):
        """Starts the application."""
        self.ui.show()
        self.ds4_handler.start()
        self.app.aboutToQuit.connect(self.cleanup)
        sys.exit(self.app.exec())

    def cleanup(self):
        print("Cleaning up...")
        self.ds4_handler.stop()
        self.serial_handler.disconnect()

if __name__ == '__main__':
    main_app = MainApplication()
    main_app.run()
