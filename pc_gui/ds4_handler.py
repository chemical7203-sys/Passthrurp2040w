import threading
import time
from inputs import get_gamepad

# This class will run in a separate thread to continuously read gamepad events
class DS4Handler(threading.Thread):
    def __init__(self, signals):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self._running = True
        self._last_states = {} # To avoid sending redundant events

    def stop(self):
        self._running = False

    def run(self):
        print("DS4 handler thread started. Waiting for gamepad...")
        while self._running:
            try:
                events = get_gamepad()
                for event in events:
                    if not self._running:
                        break
                    self._process_event(event)
            except Exception as e:
                print(f"Error: No gamepad found or an error occurred: {e}")
                print("Retrying in 3 seconds...")
                time.sleep(3)

    def _process_event(self, event):
        """Processes a single event and emits a signal if the state changed."""

        # Unique identifier for the event source (e.g., 'ABS_X')
        event_key = f"{event.ev_type}-{event.code}"

        # Only emit a signal if the value has actually changed
        if self._last_states.get(event_key) == event.state:
            return

        self._last_states[event_key] = event.state

        # --- Mapping and Signal Emitting ---

        # Sticks (0-255)
        if event.code in ['ABS_X', 'ABS_Y', 'ABS_RX', 'ABS_RY']:
            self.signals.stick_event.emit(event.code, event.state)

        # Triggers (0-255)
        elif event.code in ['ABS_Z', 'ABS_RZ']:
            self.signals.trigger_event.emit(event.code, event.state)

        # Buttons (0 for released, 1 for pressed)
        elif event.code in ['BTN_SOUTH', 'BTN_EAST', 'BTN_WEST', 'BTN_NORTH',
                            'BTN_TL', 'BTN_TR', 'BTN_SELECT', 'BTN_START',
                            'BTN_THUMBL', 'BTN_THUMBR']:
            self.signals.button_event.emit(event.code, bool(event.state))

        # D-Pad (as absolute axis, -1, 0, or 1)
        elif event.code in ['ABS_HAT0X', 'ABS_HAT0Y']:
            self.signals.dpad_event.emit(event.code, event.state)

# This block is for standalone testing and won't be used by the main app
if __name__ == '__main__':
    # A dummy signal emitter for testing
    class TestSignals:
        def stick_event(self, code, val): print(f"Stick: {code}, {val}")
        def trigger_event(self, code, val): print(f"Trigger: {code}, {val}")
        def button_event(self, code, val): print(f"Button: {code}, {val}")
        def dpad_event(self, code, val): print(f"DPad: {code}, {val}")

        # We need to create a dummy 'emit' method for each signal
        stick_event.emit = stick_event
        trigger_event.emit = trigger_event
        button_event.emit = button_event
        dpad_event.emit = dpad_event

    print("Testing DS4 Handler in standalone mode...")
    handler = DS4Handler(signals=TestSignals())
    handler.start()
    print("Handler is running. Press a button or move a stick on your controller.")
    print("Press Ctrl+C to exit.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping...")
        handler.stop()
