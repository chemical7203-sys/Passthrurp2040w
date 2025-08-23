import threading
import time
from inputs import GamePad

# This class will run in a separate thread to continuously read gamepad events
class DS4Handler(threading.Thread):
    def __init__(self, signals, device_path=None):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.device_path = device_path
        self._running = True
        self._last_states = {}

    def set_device(self, device_path):
        """Sets the device to listen to."""
        self.device_path = device_path

    def stop(self):
        self._running = False

    def run(self):
        """The main loop of the thread."""
        print("DS4 handler thread started.")
        while self._running:
            if not self.device_path:
                # Wait until a device is selected
                time.sleep(0.5)
                continue

            try:
                # Use a specific gamepad device
                gamepad = GamePad(self.device_path)
                print(f"Listening to gamepad: {self.device_path}")
                while self._running:
                    # This call blocks until an event occurs
                    events = gamepad.read()
                    for event in events:
                        if not self._running:
                            break
                        self._process_event(event)
            except Exception as e:
                print(f"Error with gamepad {self.device_path}: {e}")
                # A device might have been disconnected, wait for a new one to be selected
                self.signals.gamepad_disconnected.emit()
                self.device_path = None # Stop listening
                print("Waiting for new gamepad selection...")

    def _process_event(self, event):
        """Processes a single event and emits a signal if the state changed."""
        event_key = f"{event.ev_type}-{event.code}"

        if self._last_states.get(event_key) == event.state:
            return

        self._last_states[event_key] = event.state

        # --- Mapping and Signal Emitting ---
        if event.code in ['ABS_X', 'ABS_Y', 'ABS_RX', 'ABS_RY']:
            self.signals.stick_event.emit(event.code, event.state)
        elif event.code in ['ABS_Z', 'ABS_RZ']:
            self.signals.trigger_event.emit(event.code, event.state)
        elif event.code in ['BTN_SOUTH', 'BTN_EAST', 'BTN_WEST', 'BTN_NORTH',
                            'BTN_TL', 'BTN_TR', 'BTN_SELECT', 'BTN_START',
                            'BTN_THUMBL', 'BTN_THUMBR']:
            self.signals.button_event.emit(event.code, bool(event.state))
        elif event.code in ['ABS_HAT0X', 'ABS_HAT0Y']:
            self.signals.dpad_event.emit(event.code, event.state)

# Standalone testing is no longer practical as it requires a specific device path
if __name__ == '__main__':
    print("This module is not meant to be run standalone anymore.")
    print("Please run main.py")
