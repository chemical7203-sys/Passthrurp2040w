import threading
import time
from queue import Queue, Empty
from pyjoycon import JoyCon, get_R_id, get_L_id

class DS4Handler(threading.Thread):
    def __init__(self, signals, command_queue):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.command_queue = command_queue
        self.joycon = None
        self.joycon_id = None

    def _set_device(self, joystick_index):
        print(f"DEBUG: DS4Handler: Received SET_DEVICE command for index {joystick_index}")
        if self.joycon:
            self.joycon = None
            self.joycon_id = None

        if joystick_index is not None:
            try:
                # For now, just get the first right joycon
                self.joycon_id = get_R_id()
                if not self.joycon_id:
                    self.joycon_id = get_L_id()

                if self.joycon_id:
                    self.joycon = JoyCon(*self.joycon_id)
                    print(f"DEBUG: DS4Handler: Successfully initialized joystick: {self.joycon.get_status()['device_type']}")
                    self.signals.gamepad_connected.emit()
                else:
                    print("ERROR: DS4Handler: No Joy-Con or Pro Controller found.")
                    self.signals.gamepad_disconnected.emit()

            except Exception as e:
                print(f"ERROR: DS4Handler: Exception while setting device: {e}")
                self.joycon = None
                self.joycon_id = None
                self.signals.gamepad_disconnected.emit()
        else:
            print("DEBUG: DS4Handler: Joystick index is None, device cleared.")


    def _refresh_devices(self):
        """Emits a signal with device info."""
        print("DEBUG: DS4Handler: Received REFRESH_DEVICES command.")
        gamepads_info = []
        try:
            # This is a simplification. joycon-python doesn't have a simple way to list all devices.
            # We will just look for one.
            joycon_id = get_R_id()
            if joycon_id:
                gamepads_info.append({'name': 'Nintendo Switch Pro Controller / Right Joy-Con', 'index': 0})
            joycon_id = get_L_id()
            if joycon_id:
                gamepads_info.append({'name': 'Left Joy-Con', 'index': 1})

        except Exception as e:
            print(f"DEBUG: DS4Handler: Could not get info for joystick: {e}")

        print(f"DEBUG: DS4Handler: Emitting gamepad list: {gamepads_info}")
        self.signals.gamepad_list_updated.emit(gamepads_info)


    def run(self):
        running = True
        while running:
            try:
                command = self.command_queue.get_nowait()
                print(f"DEBUG: DS4Handler: Command received from queue: {command}")
                cmd_type = command.get('type')

                if cmd_type == 'SET_DEVICE':
                    self._set_device(command.get('index'))
                elif cmd_type == 'REFRESH_DEVICES':
                    self._refresh_devices()
                elif cmd_type == 'STOP':
                    running = False
            except Empty:
                pass

            if self.joycon:
                self._process_game_status()

            time.sleep(0.01)

        print("DEBUG: DS4Handler: Thread stopped.")

    def _process_game_status(self):
        status = self.joycon.get_status()
        self.signals.raw_event.emit(str(status))

        # Buttons
        buttons = status['buttons']['shared']
        buttons.update(status['buttons']['right'])
        buttons.update(status['buttons']['left'])

        button_map = {
            'a': 'BTN_SOUTH', 'b': 'BTN_EAST', 'x': 'BTN_WEST', 'y': 'BTN_NORTH',
            'l': 'BTN_TL', 'r': 'BTN_TR', 'zl': 'BTN_TL2', 'zr': 'BTN_TR2',
            'l-stick': 'BTN_THUMBL', 'r-stick': 'BTN_THUMBR',
            'up': 'DPAD_UP', 'down': 'DPAD_DOWN', 'left': 'DPAD_LEFT', 'right': 'DPAD_RIGHT',
            'plus': 'BTN_START', 'minus': 'BTN_SELECT', 'home': 'BTN_MODE'
        }
        for btn, pressed in buttons.items():
            if btn in button_map:
                self.signals.button_event.emit(button_map[btn], pressed)

        # Analog sticks
        left_stick = status['analog-sticks']['left']
        right_stick = status['analog-sticks']['right']
        self.signals.stick_event.emit('ABS_X', (left_stick['horizontal'] / 4095.0) * 2 - 1)
        self.signals.stick_event.emit('ABS_Y', (left_stick['vertical'] / 4095.0) * 2 - 1)
        self.signals.stick_event.emit('ABS_RX', (right_stick['horizontal'] / 4095.0) * 2 - 1)
        self.signals.stick_event.emit('ABS_RY', (right_stick['vertical'] / 4095.0) * 2 - 1)

        # IMU
        accel = status['accel']
        gyro = status['gyro']
        self.signals.imu_event.emit(accel['x'], accel['y'], accel['z'], gyro['x'], gyro['y'], gyro['z'])
