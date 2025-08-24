import threading
import time
import pygame

class DS4Handler(threading.Thread):
    """
    This class runs in a separate thread to continuously read gamepad events
    using the Pygame library.
    """
    def __init__(self, signals):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.joystick_index = None
        self.joystick = None
        self._running = True

    def set_device(self, joystick_index):
        """Sets the device index to listen to."""
        self.joystick_index = joystick_index
        # Signal the thread to re-initialize the joystick
        if self.joystick:
            self.joystick.quit()
            self.joystick = None

    def stop(self):
        self._running = False

    def run(self):
        """The main loop of the thread."""
        print("Pygame handler thread started.")
        pygame.init()
        pygame.joystick.init()

        while self._running:
            if self.joystick_index is None:
                time.sleep(0.5)
                continue

            try:
                if self.joystick is None:
                    self.joystick = pygame.joystick.Joystick(self.joystick_index)
                    self.joystick.init()
                    print(f"Listening to gamepad index: {self.joystick_index} ({self.joystick.get_name()})")

                # Pygame's event loop
                for event in pygame.event.get():
                    if not self._running:
                        break
                    self._process_event(event)

                time.sleep(0.01) # Small sleep to prevent busy-looping

            except pygame.error as e:
                print(f"Error with gamepad index {self.joystick_index}: {e}")
                self.signals.gamepad_disconnected.emit()
                self.joystick_index = None
                self.joystick = None
                print("Waiting for new gamepad selection...")

        pygame.quit()
        print("Pygame handler thread stopped.")

    def _process_event(self, event):
        """Processes a single pygame event and emits a signal."""
        if event.type == pygame.JOYAXISMOTION:
            # Axis 0: Left Stick X, Axis 1: Left Stick Y
            # Axis 2: L2 Trigger, Axis 3: Right Stick X, Axis 4: Right Stick Y, Axis 5: R2 Trigger
            # Pygame axes are -1.0 to 1.0. Triggers are -1.0 (released) to 1.0 (pressed).
            # Emit the raw float value (-1.0 to 1.0)
            if event.axis == 0: self.signals.stick_event.emit('ABS_X', event.value)
            elif event.axis == 1: self.signals.stick_event.emit('ABS_Y', event.value)

            # Triggers are also axes. Let's emit them as floats too.
            # They are -1.0 (released) to 1.0 (fully pressed)
            elif event.axis == 2: self.signals.trigger_event.emit('ABS_Z', event.value) # L2
            elif event.axis == 5: self.signals.trigger_event.emit('ABS_RZ', event.value) # R2

        elif event.type == pygame.JOYBUTTONDOWN or event.type == pygame.JOYBUTTONUP:
            pressed = (event.type == pygame.JOYBUTTONDOWN)
            # Mapping based on common DS4 layout in pygame
            button_map = {
                0: 'BTN_SOUTH', # X
                1: 'BTN_EAST',  # Circle
                2: 'BTN_WEST',  # Square
                3: 'BTN_NORTH', # Triangle
                4: 'BTN_TL',    # L1
                5: 'BTN_TR',    # R1
                8: 'BTN_SELECT',# Share
                9: 'BTN_START', # Options
                10: 'BTN_THUMBL',# L3
                11: 'BTN_THUMBR',# R3
            }
            if event.button in button_map:
                self.signals.button_event.emit(button_map[event.button], pressed)

        elif event.type == pygame.JOYHATMOTION:
            # Hat 0 is the D-Pad
            if event.hat == 0:
                x, y = event.value
                self.signals.dpad_event.emit('ABS_HAT0X', x)
                self.signals.dpad_event.emit('ABS_HAT0Y', -y) # Pygame y is inverted

# Standalone testing
if __name__ == '__main__':
    print("This module is not meant to be run standalone anymore.")
    print("Please run main.py")
