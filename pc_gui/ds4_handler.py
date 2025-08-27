import threading
import time
import pygame
import os
from queue import Queue, Empty

class DS4Handler(threading.Thread):
    def __init__(self, signals, command_queue):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.command_queue = command_queue
        self.joystick = None

    def _initialize_pygame(self):
        """Initializes Pygame and its subsystems. Must be called from the thread."""
        try:
            os.environ['SDL_VIDEODRIVER'] = 'dummy'
            pygame.init()
            pygame.joystick.init()
            return True
        except Exception as e:
            print(f"FATAL: DS4Handler: Pygame failed to initialize: {e}")
            return False

    def _set_device(self, joystick_index):
        """Sets or clears the active joystick device. Must be called from within the thread."""
        if self.joystick:
            try:
                self.joystick.quit()
            except pygame.error as e:
                print(f"ERROR: DS4Handler: Pygame error while quitting joystick: {e}")
            finally:
                self.joystick = None

        if joystick_index is not None:
            try:
                if pygame.joystick.get_count() > joystick_index:
                    self.joystick = pygame.joystick.Joystick(joystick_index)
                    self.joystick.init()
                else:
                    self.signals.gamepad_disconnected.emit()
            except pygame.error as e:
                self.joystick = None
                self.signals.gamepad_disconnected.emit()

    def _handle_events(self):
        """Polls and processes all Pygame events. Must be called from the thread."""
        try:
            for event in pygame.event.get():
                if event.type == pygame.JOYDEVICEADDED or event.type == pygame.JOYDEVICEREMOVED:
                    # Simply signal the main thread that the device list has changed.
                    # Do NOT modify any pygame state from within the event loop.
                    self.signals.device_changed.emit()
                    return

                if self.joystick and self.joystick.get_init():
                    if hasattr(event, 'instance_id') and event.instance_id == self.joystick.get_instance_id():
                        self._process_game_event(event)
        except pygame.error as e:
            self.joystick = None
            self.signals.gamepad_disconnected.emit()

    def run(self):
        """The main loop for the thread."""
        if not self._initialize_pygame():
            return

        running = True
        while running:
            try:
                command = self.command_queue.get_nowait()
                cmd_type = command.get('type')

                if cmd_type == 'SET_DEVICE':
                    self._set_device(command.get('index'))
                elif cmd_type == 'STOP':
                    running = False
            except Empty:
                pass

            if self.joystick:
                self._handle_events()

            time.sleep(0.01)

        pygame.quit()

    def _process_game_event(self, event):
        """Processes a single joystick input event."""
        self.signals.raw_event.emit(str(event))

        if event.type == pygame.JOYAXISMOTION:
            axis_map = {0: 'ABS_X', 1: 'ABS_Y', 2: 'ABS_RX', 3: 'ABS_RY', 4: 'ABS_Z', 5: 'ABS_RZ'}
            if event.axis in axis_map:
                if event.axis in [4, 5]: self.signals.trigger_event.emit(axis_map[event.axis], event.value)
                else: self.signals.stick_event.emit(axis_map[event.axis], event.value)

        elif event.type == pygame.JOYBUTTONDOWN or event.type == pygame.JOYBUTTONUP:
            pressed = (event.type == pygame.JOYBUTTONDOWN)
            button_map = {
                0: 'BTN_SOUTH', 1: 'BTN_EAST', 2: 'BTN_WEST', 3: 'BTN_NORTH',
                9: 'BTN_TL', 10: 'BTN_TR',
                7: 'BTN_THUMBL', 8: 'BTN_THUMBR',
                11: 'DPAD_UP', 12: 'DPAD_DOWN', 13: 'DPAD_LEFT', 14: 'DPAD_RIGHT',
                6: 'BTN_START', 4: 'BTN_SELECT', 5: 'BTN_MODE'
            }
            if event.button in button_map:
                self.signals.button_event.emit(button_map[event.button], pressed)
