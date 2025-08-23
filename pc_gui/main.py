import sys
from PyQt6.QtWidgets import QApplication, QMessageBox
from gamepad_ui import GamepadUI, GamepadSignals
from ds4_handler import DS4Handler
from serial_handler import SerialHandler

class MainApplication:
    """The main class that orchestrates the UI, gamepad handler, and serial handler."""
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.ui = GamepadUI()
        self.gamepad_signals = GamepadSignals()
        self.ds4_handler = DS4Handler(self.gamepad_signals)
        self.serial_handler = SerialHandler()

        # Store the last known state to send over serial
        self.last_serial_state = {'buttons': 0, 'x': 0, 'y': 0}

        self.connect_signals()

    def connect_signals(self):
        """Connects signals from the UI and gamepad handler to the appropriate slots."""
        # UI signals
        self.ui.connect_button.clicked.connect(self.toggle_serial_connection)

        # Gamepad signals for UI updates
        self.gamepad_signals.stick_event.connect(self.ui.update_stick)
        self.gamepad_signals.trigger_event.connect(self.ui.update_trigger)
        self.gamepad_signals.button_event.connect(self.ui.update_button)
        self.gamepad_signals.dpad_event.connect(self.ui.update_dpad)

        # Gamepad signals for serial transmission
        self.gamepad_signals.stick_event.connect(self.handle_gamepad_event)
        self.gamepad_signals.button_event.connect(self.handle_gamepad_event)
        # We can connect other events too if the protocol is expanded

    def toggle_serial_connection(self):
        """Connects or disconnects the serial port based on current state."""
        if self.serial_handler.ser and self.serial_handler.ser.is_open:
            self.serial_handler.disconnect()
            self.ui.connect_button.setText("Connect")
        else:
            port = self.ui.port_input.text()
            if self.serial_handler.connect(port):
                self.ui.connect_button.setText("Disconnect")
            else:
                QMessageBox.critical(self.ui, "Connection Error", f"Failed to connect to port {port}.")

    def handle_gamepad_event(self, event_code, event_value):
        """
        This slot receives all gamepad events, updates the state for serial
        transmission, and sends the data.
        """
        # Map DS4 event to our simple v1 protocol state
        if event_code == 'BTN_SOUTH':
            if event_value: # pressed
                self.last_serial_state['buttons'] |= 1
            else: # released
                self.last_serial_state['buttons'] &= ~1
        elif event_code == 'ABS_X':
            # Convert 0-255 range to -127 to 127
            self.last_serial_state['x'] = event_value - 128
        elif event_code == 'ABS_Y':
            self.last_serial_state['y'] = event_value - 128

        # Send the latest full state
        self.serial_handler.send_gamepad_state(
            self.last_serial_state['buttons'],
            self.last_serial_state['x'],
            self.last_serial_state['y']
        )

    def run(self):
        """Starts the application."""
        self.ui.show()
        self.ds4_handler.start()

        # Set a cleanup hook for when the application closes
        self.app.aboutToQuit.connect(self.cleanup)

        sys.exit(self.app.exec())

    def cleanup(self):
        """Ensures background threads and connections are closed gracefully."""
        print("Cleaning up...")
        self.ds4_handler.stop()
        self.serial_handler.disconnect()

if __name__ == '__main__':
    main_app = MainApplication()
    main_app.run()
