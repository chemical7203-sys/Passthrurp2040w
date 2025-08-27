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
        """Initializes Pygame and its subsystems."""
        print("DEBUG: Initializing Pygame...")
        try:
            os.environ['SDL_VIDEODRIVER'] = 'dummy'
            pygame.init()
            pygame.joystick.init()
            print(f"DEBUG: Pygame initialized. Joystick count: {pygame.joystick.get_count()}")
            return True
        except Exception as e:
            print(f"FATAL: Pygame failed to initialize: {e}")
            return False

    def _set_device(self, joystick_index):
        """Sets the active joystick device. Must be called from within the thread."""
        print(f"DEBUG: Command received: SET_DEVICE to index {joystick_index}")
        try:
            if self.joystick:
                print(f"DEBUG: Quitting previous joystick (instance ID: {self.joystick.get_instance_id()})")
                self.joystick.quit()
                self.joystick = None

            if joystick_index is not None:
                print(f"DEBUG: Setting new joystick to index {joystick_index}")
                self.joystick = pygame.joystick.Joystick(joystick_index)
                self.joystick.init()
                print(f"DEBUG: Successfully set joystick to index: {joystick_index} ({self.joystick.get_name()})")
        except Exception as e:
            print(f"ERROR: Failed to set device to index {joystick_index}: {e}")
            self.joystick = None
            self.signals.gamepad_disconnected.emit()

    def _handle_events(self):
        """Handles all Pygame events."""
        if not self.joystick:
            return

        try:
            for event in pygame.event.get():
                # print(f"DEBUG: Pygame event: {event}") # This can be very noisy
                if event.type == pygame.JOYDEVICEADDED or event.type == pygame.JOYDEVICEREMOVED:
                    print(f"DEBUG: Hot-plug event detected: {event}. Signaling main thread.")
                    self.signals.device_changed.emit()
                    self.joystick = None # Invalidate joystick
                    return # Exit event loop to allow re-selection

                if self.joystick: # Check if joystick is still valid
                    self._process_event(event)

        except Exception as e:
            print(f"ERROR: Exception in event loop: {e}")
            self.joystick = None
            self.signals.gamepad_disconnected.emit()

    def run(self):
        """The main loop for the thread."""
        if not self._initialize_pygame():
            return

        running = True
        while running:
            try:
                # Check for commands from the main thread
                command = self.command_queue.get_nowait()
                cmd_type = command.get('type')

                if cmd_type == 'SET_DEVICE':
                    self._set_device(command.get('index'))
                elif cmd_type == 'STOP':
                    print("DEBUG: STOP command received. Shutting down thread.")
                    running = False
            except Empty:
                pass # No command in queue, proceed with event handling
            except Exception as e:
                print(f"ERROR: Error processing command queue: {e}")

            if self.joystick:
                self._handle_events()

            time.sleep(0.01) # Prevent high CPU usage

        print("DEBUG: Quitting Pygame...")
        pygame.quit()
        print("DEBUG: Pygame handler thread stopped.")

    def _process_event(self, event):
        """Processes a single joystick event."""
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
                9: 'BTN_TL', 10: 'BTN_TR', 7: 'BTN_THUMBL', 8: 'BTN_THUMBR',
                11: 'DPAD_UP', 12: 'DPAD_DOWN', 13: 'DPAD_LEFT', 14: 'DPAD_RIGHT',
                6: 'BTN_START', 4: 'BTN_SELECT', 5: 'BTN_MODE'
            }
            if event.button in button_map:
                self.signals.button_event.emit(button_map[event.button], pressed)
