import threading
import time
import hid
from queue import Queue, Empty

# Sony DualShock 4 V2
DS4_VENDOR_ID = 0x054C
DS4_PRODUCT_ID = 0x09CC
DS4_PRODUCT_ID_V1 = 0x05C4 # V1 DS4 controller

class DS4Handler(threading.Thread):
    def __init__(self, signals, command_queue, serial_state):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.command_queue = command_queue
        self.serial_state = serial_state # Shared state dictionary
        self.device = None
        self.running = False
        self.last_button_state = {}

    def _find_device(self):
        """Finds the DS4 controller."""
        print("DEBUG: DS4Handler: Searching for DS4 controller...")
        devices = hid.enumerate(DS4_VENDOR_ID, DS4_PRODUCT_ID)
        if not devices:
            devices = hid.enumerate(DS4_VENDOR_ID, DS4_PRODUCT_ID_V1)

        if devices:
            path = devices[0]['path']
            self.device = hid.device()
            self.device.open_path(path)
            self.device.set_nonblocking(True)
            print(f"DEBUG: DS4Handler: Found DS4 controller at {path}")
            # Emit a signal to update the UI
            self.signals.gamepad_list_updated.emit([{'name': 'Sony DS4 Controller', 'index': 0}])
            return True
        else:
            print("ERROR: DS4Handler: No DS4 controller found.")
            self.signals.gamepad_list_updated.emit([])
            return False

    def run(self):
        self.running = True
        if not self._find_device():
            self.running = False

        while self.running:
            try:
                command = self.command_queue.get_nowait()
                if command.get('type') == 'STOP':
                    self.running = False
                elif command.get('type') == 'REFRESH_DEVICES':
                    if self.device:
                        self.device.close()
                    self._find_device()
            except Empty:
                pass

            if self.device:
                report = self.device.read(64)
                if report:
                    # DS4 Bluetooth report ID is 0x11, USB is 0x01.
                    # We only care about the full report with IMU data.
                    if report[0] == 0x11 or report[0] == 0x01:
                        self._parse_ds4_report(report)
            else:
                # If no device, sleep a bit to avoid busy-waiting
                time.sleep(1)

        if self.device:
            self.device.close()
        print("DEBUG: DS4Handler: Thread stopped.")

    def _parse_ds4_report(self, report):
        """Parses the DS4 HID report and updates the shared state."""
        # Byte offsets for USB (report[0] == 0x01)
        lx_off, ly_off, rx_off, ry_off = 1, 2, 3, 4
        btn_off, hat_off = 5, 5
        l2_off, r2_off = 8, 9
        # Gyro and Accel are at different offsets for USB vs BT
        # USB has timestamp and other data before IMU
        if report[0] == 0x11: # Bluetooth
            lx_off, ly_off, rx_off, ry_off = 3, 4, 5, 6
            btn_off, hat_off = 7, 7
            l2_off, r2_off = 10, 11
            accel_off = 15
            gyro_off = 21
        else: # USB
            accel_off = 13
            gyro_off = 19

        # Analog Sticks (0-255, centered at 128)
        self.serial_state['lx'] = report[lx_off] - 128
        self.serial_state['ly'] = report[ly_off] - 128
        self.serial_state['rx'] = report[rx_off] - 128
        self.serial_state['ry'] = report[ry_off] - 128

        # Triggers (0-255)
        self.serial_state['l2'] = report[l2_off]
        self.serial_state['r2'] = report[r2_off]

        # D-Pad (as a bitmask, similar to the original dpad state)
        dpad_val = report[hat_off] & 0x0F
        dpad_map = {0: 1, 1: 9, 2: 8, 3: 12, 4: 4, 5: 6, 6: 2, 7: 3} # Map 8-way hat to 4-bit mask
        self.serial_state['dpad'] = dpad_map.get(dpad_val, 0)

        # Buttons
        buttons_b1 = report[btn_off]
        buttons_b2 = report[btn_off + 1]
        buttons_b3 = report[btn_off + 2]

        # Mapping based on `main.py`'s original map
        # 'BTN_SOUTH': 1<<0, 'BTN_EAST': 1<<1, 'BTN_WEST': 1<<2, 'BTN_NORTH': 1<<3,
        # 'BTN_TL': 1<<4, 'BTN_TR': 1<<5, 'BTN_TL2': 1<<6, 'BTN_TR2': 1<<7,
        # 'BTN_SELECT': 1<<8, 'BTN_START': 1<<9, 'BTN_THUMBL': 1<<10, 'BTN_THUMBR': 1<<11,
        # 'BTN_MODE': 1<<12

        current_buttons = 0
        if buttons_b1 & 0x10: current_buttons |= (1 << 2) # Square -> West
        if buttons_b1 & 0x20: current_buttons |= (1 << 0) # Cross -> South
        if buttons_b1 & 0x40: current_buttons |= (1 << 1) # Circle -> East
        if buttons_b1 & 0x80: current_buttons |= (1 << 3) # Triangle -> North
        if buttons_b2 & 0x01: current_buttons |= (1 << 4) # L1
        if buttons_b2 & 0x02: current_buttons |= (1 << 5) # R1
        if buttons_b2 & 0x04: self.serial_state['l2'] = 255 # L2 Button
        if buttons_b2 & 0x08: self.serial_state['r2'] = 255 # R2 Button
        if buttons_b2 & 0x10: current_buttons |= (1 << 8) # Share -> Select
        if buttons_b2 & 0x20: current_buttons |= (1 << 9) # Options -> Start
        if buttons_b2 & 0x40: current_buttons |= (1 << 10) # L3
        if buttons_b2 & 0x80: current_buttons |= (1 << 11) # R3
        if buttons_b3 & 0x01: current_buttons |= (1 << 12) # PS Button -> Home

        self.serial_state['buttons'] = current_buttons

        # IMU Data (16-bit signed little-endian)
        self.serial_state['accel_x'] = int.from_bytes(report[accel_off:accel_off+2], 'little', signed=True)
        self.serial_state['accel_y'] = int.from_bytes(report[accel_off+2:accel_off+4], 'little', signed=True)
        self.serial_state['accel_z'] = int.from_bytes(report[accel_off+4:accel_off+6], 'little', signed=True)
        self.serial_state['gyro_x'] = int.from_bytes(report[gyro_off:gyro_off+2], 'little', signed=True)
        self.serial_state['gyro_y'] = int.from_bytes(report[gyro_off+2:gyro_off+4], 'little', signed=True)
        self.serial_state['gyro_z'] = int.from_bytes(report[gyro_off+4:gyro_off+6], 'little', signed=True)

        # --- Emit signals for UI update (optional, but good for consistency) ---
        # This part is more complex as we don't have event-based updates anymore.
        # We can compare with the last state and emit signals for changes.
        # For now, we'll just update the shared state directly. The UI might not
        # reflect the changes perfectly without this, but the core functionality
        # of sending data to the Pico will work.
