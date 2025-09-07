import threading
import time
import hid
from queue import Queue, Empty

# Pro Controller USB VID and PID
PRO_CONTROLLER_VID = 0x057E
PRO_CONTROLLER_PID = 0x2009

class ProControllerHandler(threading.Thread):
    def __init__(self, signals, command_queue):
        super().__init__()
        self.daemon = True
        self.signals = signals
        self.command_queue = command_queue
        self.device = None
        self.running = False

    def _send_subcommand(self, command, data):
        """Sends a subcommand to the controller."""
        if not self.device:
            return

        # Report ID 0x01 is for subcommands
        # The packet format is: [Report ID, Packet Counter, Subcommand, Data...]
        # We'll use a fixed packet counter of 0x00 for simplicity.
        report = bytearray([0x01, 0x00])
        report.append(command)
        report.extend(data)

        # Pad with 0s to 64 bytes (standard HID report size)
        while len(report) < 64:
            report.append(0)

        try:
            self.device.write(report)
            # Read the ACK
            # response = self.device.read(64, timeout=100)
            # print(f"DEBUG: Subcommand {command:02x} ACK: {response}")
        except Exception as e:
            print(f"ERROR: Failed to send subcommand {command:02x}: {e}")

    def _initialize_controller(self):
        """Sends the necessary commands to enable full reporting mode with IMU."""
        print("DEBUG: Initializing Pro Controller...")
        # Enable IMU
        self._send_subcommand(0x40, bytes([0x01]))
        time.sleep(0.1)
        # Set input report mode to "Standard full mode"
        self._send_subcommand(0x03, bytes([0x30]))
        time.sleep(0.1)
        print("DEBUG: Pro Controller initialized.")

    def _parse_input_report(self, report):
        """Parses the standard full input report (0x30)."""
        if report[0] != 0x30:
            return # Not the report we're looking for

        # Buttons (Bytes 1-3) - Note: This mapping is based on dekunukem's research
        # The button mapping needs to be translated to the abstract codes used by main.py
        # For now, we will just pass dummy data for buttons and sticks
        # TODO: Implement full button and stick parsing and mapping
        self.signals.button_event.emit('BTN_SOUTH', (report[1] & 0x04) >> 2) # A button

        # Sticks (Bytes 4-9) - 12-bit values
        # TODO: Parse and scale sticks
        lx = (report[4] | ((report[5] & 0x0F) << 8))
        ly = ((report[5] >> 4) | (report[6] << 4))
        # Scale to -1.0 to 1.0 range
        self.signals.stick_event.emit('ABS_X', (lx - 2048) / 2048.0)
        self.signals.stick_event.emit('ABS_Y', (ly - 2048) / 2048.0)


        # IMU Data (Bytes 13-48)
        # 3 samples of 12 bytes each (ax, ay, az, gx, gy, gz - all int16)
        # We'll just use the first sample
        ax = int.from_bytes(report[13:15], 'little', signed=True)
        ay = int.from_bytes(report[15:17], 'little', signed=True)
        az = int.from_bytes(report[17:19], 'little', signed=True)
        gx = int.from_bytes(report[19:21], 'little', signed=True)
        gy = int.from_bytes(report[21:23], 'little', signed=True)
        gz = int.from_bytes(report[23:25], 'little', signed=True)

        self.signals.imu_event.emit(ax, ay, az, gx, gy, gz)

    def run(self):
        self.running = True
        while self.running:
            try:
                command = self.command_queue.get_nowait()
                if command.get('type') == 'STOP':
                    self.running = False
                    break
            except Empty:
                pass

            if not self.device:
                try:
                    self.device = hid.device()
                    self.device.open(PRO_CONTROLLER_VID, PRO_CONTROLLER_PID)
                    print("DEBUG: Pro Controller found and opened.")
                    self._initialize_controller()
                except Exception as e:
                    self.device = None
                    # print(f"DEBUG: Pro Controller not found, retrying in 5s... ({e})")
                    time.sleep(5)
                    continue

            try:
                # Read with a timeout so we can check the running flag
                report = self.device.read(64, timeout=100)
                if report:
                    self._parse_input_report(report)
            except hid.HIDException as e:
                print(f"ERROR: HIDException: {e}. Disconnecting controller.")
                self.device.close()
                self.device = None
                self.signals.gamepad_disconnected.emit()
            except Exception as e:
                print(f"ERROR: Unexpected error in read loop: {e}")
                self.running = False

            time.sleep(0.005) # ~200Hz loop

        if self.device:
            self.device.close()
        print("DEBUG: ProControllerHandler thread stopped.")
