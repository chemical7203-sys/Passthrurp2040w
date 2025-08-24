import threading
import time
import pygame

class DS4Handler(threading.Thread):
    def __init__(self, signals):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.joystick_index = None
        self.joystick = None
        self._running = True

    def set_device(self, joystick_index):
        self.joystick_index = joystick_index
        if self.joystick:
            self.joystick.quit()
            self.joystick = None

    def stop(self):
        self._running = False

    def run(self):
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

                for event in pygame.event.get():
                    if not self._running: break
                    self._process_event(event)
                time.sleep(0.01)

            except pygame.error as e:
                print(f"Error with gamepad index {self.joystick_index}: {e}")
                self.signals.gamepad_disconnected.emit()
                self.joystick_index = None
                self.joystick = None
                print("Waiting for new gamepad selection...")

        pygame.quit()
        print("Pygame handler thread stopped.")

    def _process_event(self, event):
        self.signals.raw_event.emit(str(event))

        if event.type == pygame.JOYAXISMOTION:
            # Corrected Axis Mapping based on user feedback
            axis_map = {
                0: 'ABS_X',    # Left Stick X
                1: 'ABS_Y',    # Left Stick Y
                2: 'ABS_RX',   # Right Stick X
                3: 'ABS_RY',   # Right Stick Y
                4: 'ABS_Z',    # L2 Trigger
                5: 'ABS_RZ',   # R2 Trigger
            }
            if event.axis in axis_map:
                if event.axis in [4, 5]:
                    self.signals.trigger_event.emit(axis_map[event.axis], event.value)
                else:
                    self.signals.stick_event.emit(axis_map[event.axis], event.value)

        elif event.type == pygame.JOYBUTTONDOWN or event.type == pygame.JOYBUTTONUP:
            pressed = (event.type == pygame.JOYBUTTONDOWN)
            # Corrected Button Mapping based on user feedback
            button_map = {
                0: 'BTN_SOUTH', 1: 'BTN_EAST', 2: 'BTN_WEST', 3: 'BTN_NORTH',
                9: 'BTN_TL', 10: 'BTN_TR',
                7: 'BTN_THUMBL', 8: 'BTN_THUMBR',
                11: 'DPAD_UP', 12: 'DPAD_DOWN', 13: 'DPAD_LEFT', 14: 'DPAD_RIGHT',
                # Assuming 6 is Start and 4 is Select for DS4
                6: 'BTN_START', 4: 'BTN_SELECT'
            }
            if event.button in button_map:
                self.signals.button_event.emit(button_map[event.button], pressed)

        # JOYHATMOTION is not used for this controller
        elif event.type == pygame.JOYHATMOTION:
            pass
