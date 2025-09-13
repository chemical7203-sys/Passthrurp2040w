import threading
import time
import hid
import struct
from queue import Queue, Empty

# DS4 Vendor ID and Product IDs
SONY_VID = 0x054C
DS4_PID = 0x05C4
DS4_V2_PID = 0x09CC

# Mapping from DS4 HAT value to DPAD button states
DPAD_MAP = {
    0: {'DPAD_UP'}, 1: {'DPAD_UP', 'DPAD_RIGHT'}, 2: {'DPAD_RIGHT'},
    3: {'DPAD_DOWN', 'DPAD_RIGHT'}, 4: {'DPAD_DOWN'}, 5: {'DPAD_DOWN', 'DPAD_LEFT'},
    6: {'DPAD_LEFT'}, 7: {'DPAD_UP', 'DPAD_LEFT'}, 8: set()  # 8 is neutral
}

# Mapping from report byte/bit to button name for standard buttons
BUTTON_MAP = [
    (5, 1 << 4, 'BTN_WEST'),   # Square
    (5, 1 << 5, 'BTN_SOUTH'),  # Cross
    (5, 1 << 6, 'BTN_EAST'),   # Circle
    (5, 1 << 7, 'BTN_NORTH'),  # Triangle
    (6, 1 << 0, 'BTN_TL'),     # L1
    (6, 1 << 1, 'BTN_TR'),     # R1
    (6, 1 << 2, 'BTN_TL2'),    # L2 Button Press
    (6, 1 << 3, 'BTN_TR2'),    # R2 Button Press
    (6, 1 << 4, 'BTN_SELECT'), # Share
    (6, 1 << 5, 'BTN_START'),  # Options
    (6, 1 << 6, 'BTN_THUMBL'), # L3
    (6, 1 << 7, 'BTN_THUMBR'), # R3
    (7, 1 << 0, 'BTN_MODE'),   # PS Button
    (7, 1 << 1, 'TPAD_CLICK')# Touchpad Click
]

class DS4Handler(threading.Thread):
    def __init__(self, signals, command_queue):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.command_queue = command_queue
        self.device = None
        self.last_report = None
        self.running = True

    def _set_device(self, device_path):
        """Opens or closes the specified HID device based on its path."""
        if self.device:
            self.device.close()
            self.device = None
            self.last_report = None

        if device_path:
            try:
                self.device = hid.device()
                self.device.open_path(device_path)
                self.device.set_nonblocking(1)

                # Send a feature report to enable motion sensing
                try:
                    # Report ID 0x02 is used by some DS4 models/firmwares to enable full reports (0x11)
                    # which include the gyro/accelerometer data.
                    self.device.send_feature_report(b'\x02')
                except Exception as e:
                    # This might fail on some models/platforms, but it's not critical.
                    # The read loop will still work, just potentially without motion data.
                    print(f"WARNING: DS4Handler: Could not send feature report to enable motion: {e}")

            except Exception as e:
                print(f"ERROR: DS4Handler: Failed to open HID device at {device_path}: {e}")
                self.device = None
                self.signals.gamepad_disconnected.emit()
        else:
            # This is called when deselecting a device
            self.signals.gamepad_disconnected.emit()

    def _refresh_devices(self):
        """Scans for DS4 gamepads and emits a signal with their info."""
        devices = hid.enumerate(SONY_VID, 0)
        ds4_devices = []
        for dev in devices:
            if dev['product_id'] in [DS4_PID, DS4_V2_PID]:
                device_info = {'name': dev['product_string'], 'path': dev['path']}
                ds4_devices.append(device_info)
        self.signals.gamepad_list_updated.emit(ds4_devices)

    def run(self):
        """Main thread loop: processes commands and reads HID reports."""
        while self.running:
            try:
                command = self.command_queue.get_nowait()
                cmd_type = command.get('type')

                if cmd_type == 'SET_DEVICE':
                    self._set_device(command.get('path'))
                elif cmd_type == 'REFRESH_DEVICES':
                    self._refresh_devices()
                elif cmd_type == 'STOP':
                    self.running = False
            except Empty:
                pass

            if self.device:
                try:
                    report = self.device.read(64)
                    if report and report[0] == 0x01:
                        self._parse_hid_report(bytes(report))
                except Exception as e:
                    print(f"ERROR: DS4Handler: HID read error: {e}. Disconnecting.")
                    self._set_device(None)

            time.sleep(0.002)

        if self.device:
            self.device.close()

    def _parse_hid_report(self, report):
        """Parses the 64-byte HID report and emits signals only on state changes."""
        if self.last_report == report:
            return

        lx, ly, rx, ry = [(v - 128) / 128.0 for v in report[1:5]]
        if not self.last_report or lx != ((self.last_report[1] - 128) / 128.0): self.signals.stick_event.emit('ABS_X', lx)
        if not self.last_report or ly != ((self.last_report[2] - 128) / 128.0): self.signals.stick_event.emit('ABS_Y', ly)
        if not self.last_report or rx != ((self.last_report[3] - 128) / 128.0): self.signals.stick_event.emit('ABS_RX', rx)
        if not self.last_report or ry != ((self.last_report[4] - 128) / 128.0): self.signals.stick_event.emit('ABS_RY', ry)

        l2_trigger, r2_trigger = [(v / 127.5) - 1.0 for v in report[8:10]]
        if not self.last_report or l2_trigger != ((self.last_report[8] / 127.5) - 1.0): self.signals.trigger_event.emit('ABS_Z', l2_trigger)
        if not self.last_report or r2_trigger != ((self.last_report[9] / 127.5) - 1.0): self.signals.trigger_event.emit('ABS_RZ', r2_trigger)

        for byte_idx, mask, name in BUTTON_MAP:
            current_state = (report[byte_idx] & mask) != 0
            last_state = (self.last_report[byte_idx] & mask) != 0 if self.last_report else not current_state
            if current_state != last_state:
                self.signals.button_event.emit(name, current_state)

        current_dpad_val = report[5] & 0x0F
        last_dpad_val = (self.last_report[5] & 0x0F) if self.last_report else -1
        if current_dpad_val != last_dpad_val:
            current_buttons = DPAD_MAP.get(current_dpad_val, set())
            last_buttons = DPAD_MAP.get(last_dpad_val, set())
            for btn in current_buttons - last_buttons: self.signals.button_event.emit(btn, True)
            for btn in last_buttons - current_buttons: self.signals.button_event.emit(btn, False)

        if not self.last_report or report[13:25] != self.last_report[13:25]:
            gyro_x, gyro_y, gyro_z, accel_x, accel_y, accel_z = struct.unpack_from('<hhhhhh', report, 13)
            if hasattr(self.signals, 'motion_event'):
                self.signals.motion_event.emit(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z)

        self.last_report = report
