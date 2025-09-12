import threading
import time
import hid
import struct
from queue import Queue, Empty

# Vendor and Product IDs for Sony DualShock 4
# 0x05C4 is for the original DS4, 0x09CC is for the newer "DS4 v2"
SONY_VID = 0x054C
DS4_PIDS = [0x05C4, 0x09CC]

class DS4Handler(threading.Thread):
    def __init__(self, signals, command_queue):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.command_queue = command_queue

        self.device = None
        self.device_path = None
        self.running = False
        self.read_thread = None

        # Store last state to only emit signals on change
        self.last_state = {}

    def _refresh_devices(self):
        """Scans for DS4 gamepads and emits a signal with their info."""
        print("DEBUG: DS4Handler: Refreshing HID devices...")
        devices = hid.enumerate(SONY_VID)
        gamepads_info = []
        for dev in devices:
            if dev['product_id'] in DS4_PIDS:
                info = {
                    'name': dev['product_string'],
                    'path': dev['path'],
                    'index': len(gamepads_info) # Use list index as a simple ID
                }
                gamepads_info.append(info)

        print(f"DEBUG: DS4Handler: Found {len(gamepads_info)} DS4 devices. Emitting list.")
        self.signals.gamepad_list_updated.emit(gamepads_info)

    def _set_device(self, device_path):
        """Sets or clears the active joystick device."""
        print(f"DEBUG: DS4Handler: Setting device to path: {device_path}")
        if self.device:
            self.running = False
            if self.read_thread and self.read_thread.is_alive():
                self.read_thread.join()
            self.device.close()
            self.device = None
            self.device_path = None

        if device_path:
            try:
                self.device_path = device_path
                self.device = hid.device()
                self.device.open_path(self.device_path)
                self.device.set_nonblocking(True)

                self.running = True
                self.read_thread = threading.Thread(target=self._read_loop)
                self.read_thread.daemon = True
                self.read_thread.start()
                print(f"DEBUG: DS4Handler: Successfully opened device.")
            except Exception as e:
                print(f"ERROR: DS4Handler: Failed to open HID device: {e}")
                self.device = None
                self.signals.gamepad_disconnected.emit()
        else:
            print("DEBUG: DS4Handler: Device cleared.")

    def _read_loop(self):
        """Continuously reads from the HID device."""
        print("DEBUG: DS4Handler: Read loop started.")
        while self.running:
            try:
                report = self.device.read(64) # DS4 report is 64 bytes
                if report:
                    self._parse_report(report)
            except hid.error as e:
                print(f"ERROR: DS4Handler: HID read error: {e}. Disconnecting.")
                self.running = False
                self.signals.gamepad_disconnected.emit()
            except Exception as e:
                print(f"ERROR: DS4Handler: Unexpected error in read loop: {e}")
            time.sleep(0.004) # ~250hz poll rate
        print("DEBUG: DS4Handler: Read loop finished.")

    def _parse_report(self, report):
        """Parses the 64-byte raw HID report and emits signals."""
        # This parsing is based on the hid_ds4_report_t struct in the firmware
        # and standard DS4 report documentation.

        # For simplicity, we'll just unpack the whole thing based on the C struct.
        # Note: This assumes the C struct and this unpack format are perfectly aligned.
        # '<' for little-endian, 'B' for uint8, 'b' for int8, 'H' for uint16, 'h' for int16
        try:
            # We only care about the first ~30 bytes for inputs
            # Unpack sticks, triggers, buttons, and motion data
            lx, ly, rx, ry, l2, r2 = struct.unpack_from('<4B2B', bytes(report), 1)
            buttons_dpad_etc = struct.unpack_from('<3B', bytes(report), 5)
            gyro_x, gyro_y, gyro_z, accel_x, accel_y, accel_z = struct.unpack_from('<6h', bytes(report), 13)

            # --- Sticks (0-255 -> -1.0 to 1.0) ---
            self.signals.stick_event.emit('ABS_X', (lx - 128) / 128.0)
            self.signals.stick_event.emit('ABS_Y', (ly - 128) / 128.0)
            self.signals.stick_event.emit('ABS_RX', (rx - 128) / 128.0)
            self.signals.stick_event.emit('ABS_RY', (ry - 128) / 128.0)

            # --- Triggers (0-255 -> -1.0 to 1.0) ---
            self.signals.trigger_event.emit('ABS_Z', (l2 / 255.0) * 2 - 1)
            self.signals.trigger_event.emit('ABS_RZ', (r2 / 255.0) * 2 - 1)

            # --- Buttons & DPAD ---
            # This is complex due to bit-packing, so we'll do it manually.
            dpad_val = buttons_dpad_etc[0] & 0x0F

            # DPAD (convert from HAT to individual button signals)
            dpad_up = dpad_val in [0, 1, 7]
            dpad_down = dpad_val in [3, 4, 5]
            dpad_left = dpad_val in [5, 6, 7]
            dpad_right = dpad_val in [1, 2, 3]
            self.signals.button_event.emit('DPAD_UP', dpad_up)
            self.signals.button_event.emit('DPAD_DOWN', dpad_down)
            self.signals.button_event.emit('DPAD_LEFT', dpad_left)
            self.signals.button_event.emit('DPAD_RIGHT', dpad_right)

            # Face Buttons
            self.signals.button_event.emit('BTN_WEST', bool(buttons_dpad_etc[0] & 0x10)) # Square
            self.signals.button_event.emit('BTN_SOUTH', bool(buttons_dpad_etc[0] & 0x20)) # Cross
            self.signals.button_event.emit('BTN_EAST', bool(buttons_dpad_etc[0] & 0x40)) # Circle
            self.signals.button_event.emit('BTN_NORTH', bool(buttons_dpad_etc[0] & 0x80)) # Triangle

            # Shoulder Buttons
            self.signals.button_event.emit('BTN_TL', bool(buttons_dpad_etc[1] & 0x01)) # L1
            self.signals.button_event.emit('BTN_TR', bool(buttons_dpad_etc[1] & 0x02)) # R1
            # L2/R2 buttons (distinct from triggers)
            # self.signals.button_event.emit('BTN_TL2', bool(buttons_dpad_etc[1] & 0x04))
            # self.signals.button_event.emit('BTN_TR2', bool(buttons_dpad_etc[1] & 0x08))

            # Other buttons
            self.signals.button_event.emit('BTN_SELECT', bool(buttons_dpad_etc[1] & 0x10)) # Share
            self.signals.button_event.emit('BTN_START', bool(buttons_dpad_etc[1] & 0x20)) # Options
            self.signals.button_event.emit('BTN_THUMBL', bool(buttons_dpad_etc[1] & 0x40)) # L3
            self.signals.button_event.emit('BTN_THUMBR', bool(buttons_dpad_etc[1] & 0x80)) # R3
            self.signals.button_event.emit('BTN_MODE', bool(buttons_dpad_etc[2] & 0x01)) # PS Button

            # --- Motion Sensors (-32768 to 32767 -> -1.0 to 1.0) ---
            # Emitting as a stick event for compatibility with the UI
            self.signals.stick_event.emit('ABS_HAT0X', accel_x / 32768.0)
            self.signals.stick_event.emit('ABS_HAT0Y', accel_y / 32768.0)
            self.signals.stick_event.emit('ABS_HAT0Z', accel_z / 32768.0)
            self.signals.stick_event.emit('ABS_HAT1X', gyro_x / 32768.0)
            self.signals.stick_event.emit('ABS_HAT1Y', gyro_y / 32768.0)
            self.signals.stick_event.emit('ABS_HAT1Z', gyro_z / 32768.0)

            # For debugging, emit raw event
            self.signals.raw_event.emit(f"HID Report: {list(report)}")

        except Exception as e:
            print(f"ERROR: DS4Handler: Failed to parse report: {e}")

    def run(self):
        """Main thread loop, handles commands from the UI."""
        print("DEBUG: DS4Handler: Main thread started.")
        self.running = True
        while self.running:
            try:
                command = self.command_queue.get(timeout=0.1)
                cmd_type = command.get('type')
                print(f"DEBUG: DS4Handler: Command received: {cmd_type}")

                if cmd_type == 'SET_DEVICE':
                    # The 'index' from the UI now corresponds to a device 'path'
                    self._set_device(command.get('path'))
                elif cmd_type == 'REFRESH_DEVICES':
                    self._refresh_devices()
                elif cmd_type == 'STOP':
                    self.running = False
            except Empty:
                pass # No command, just loop

        self._set_device(None) # Clean up device on exit
        print("DEBUG: DS4Handler: Thread stopped.")
