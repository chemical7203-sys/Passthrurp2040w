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
        """Initializes Pygame and its subsystems with detailed logging."""
        print("DEBUG: DS4Handler: Initializing Pygame...")
        try:
            os.environ['SDL_VIDEODRIVER'] = 'dummy'
            pygame.init()
            pygame.joystick.init()
            print(f"DEBUG: DS4Handler: Pygame initialized successfully.")
            return True
        except Exception as e:
            print(f"FATAL: DS4Handler: Pygame failed to initialize: {e}")
            return False

    def _set_device(self, joystick_index):
        """Sets or clears the active joystick device. Must be called from the thread."""
        print(f"DEBUG: DS4Handler: Received SET_DEVICE command for index {joystick_index}")
        if self.joystick:
            try:
                print(f"DEBUG: DS4Handler: Quitting previous joystick instance.")
                self.joystick.quit()
            except Exception as e:
                print(f"ERROR: DS4Handler: Exception while quitting joystick: {e}")
            finally:
                self.joystick = None

        if joystick_index is not None:
            try:
                count = pygame.joystick.get_count()
                print(f"DEBUG: DS4Handler: Checking joystick index {joystick_index} against count {count}")
                if count > joystick_index:
                    self.joystick = pygame.joystick.Joystick(joystick_index)
                    print(f"DEBUG: DS4Handler: Joystick object created for index {joystick_index}. Initializing...")
                    self.joystick.init()
                    print(f"DEBUG: DS4Handler: Successfully initialized joystick: {self.joystick.get_name()}")
                else:
                    print(f"ERROR: DS4Handler: Invalid joystick index {joystick_index}.")
                    self.signals.gamepad_disconnected.emit()
            except pygame.error as e:
                print(f"ERROR: DS4Handler: Pygame error while setting device: {e}")
                self.joystick = None
                self.signals.gamepad_disconnected.emit()
        else:
            print("DEBUG: DS4Handler: Joystick index is None, device cleared.")

    def _refresh_devices(self):
        """Scans for gamepads and emits a signal with the list."""
        print("DEBUG: DS4Handler: Received REFRESH_DEVICES command.")
        pygame.joystick.quit()
        pygame.joystick.init()

        gamepads = []
        count = pygame.joystick.get_count()
        print(f"DEBUG: DS4Handler: Found {count} joysticks.")
        for i in range(count):
            try:
                joystick = pygame.joystick.Joystick(i)
                # We don't need to init() to get the name.
                gamepads.append({'name': joystick.get_name(), 'index': i})
            except pygame.error:
                print(f"DEBUG: DS4Handler: Could not get info for joystick {i}.")
                continue

        print(f"DEBUG: DS4Handler: Emitting gamepad list: {gamepads}")
        self.signals.gamepad_list_updated.emit(gamepads)

    def _handle_events(self):
        try:
            for event in pygame.event.get():
                if event.type == pygame.JOYDEVICEADDED or event.type == pygame.JOYDEVICEREMOVED:
                    print(f"DEBUG: DS4Handler: Hot-plug event detected: {event}. Signaling main thread to refresh.")
                    # Only invalidate the current joystick if it's the one that was removed.
                    if self.joystick and event.type == pygame.JOYDEVICEREMOVED and event.instance_id == self.joystick.get_instance_id():
                        print(f"DEBUG: DS4Handler: Currently active joystick (instance_id={event.instance_id}) was removed.")
                        self.joystick = None
                    # Always tell the UI to refresh its list.
                    self.signals.device_changed.emit()
                    continue # Continue processing other events or next loop iteration

                if self.joystick and self.joystick.get_init():
                    if hasattr(event, 'instance_id') and event.instance_id == self.joystick.get_instance_id():
                        self._process_game_event(event)
        except pygame.error as e:
            print(f"ERROR: DS4Handler: Pygame error in event loop: {e}. Disconnecting.")
            self.joystick = None
            self.signals.gamepad_disconnected.emit()

    def run(self):
        if not self._initialize_pygame():
            return

        running = True
        while running:
            try:
                command = self.command_queue.get_nowait()
                cmd_type = command.get('type')

                if cmd_type == 'SET_DEVICE':
                    self._set_device(command.get('index'))
                elif cmd_type == 'REFRESH_DEVICES':
                    self._refresh_devices()
                elif cmd_type == 'STOP':
                    running = False
            except Empty:
                pass

            if self.joystick:
                self._handle_events()

            time.sleep(0.01)

        pygame.quit()
        print("DEBUG: DS4Handler: Thread stopped.")

    def _process_game_event(self, event):
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
