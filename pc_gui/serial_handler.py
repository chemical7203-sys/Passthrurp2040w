import serial
import struct
import time
import threading
from collections import deque

class SerialHandler:
    def __init__(self):
        self.ser = None
        self.port = None
        self.read_thread = None
        self._running = False
        self.rx_queue = deque(maxlen=100)
        # CRC8 setup
        self.CRC8Table = bytearray(256)
        self._buildCRC8Table(0x07) # Standard CRC-8 polynomial

    # --- CRC8 Implementation from hdtodd/CRC8-Library ---
    def _buildCRC8Table(self, poly):
      for i in range (0,256):
        c = i
        for j in range (0,8):
            c = c<<1 if ((c & 0x80) == 0) else (c<<1) ^ poly
            c &= 0xff
        self.CRC8Table[i] = c

    def _crc8(self, msg, init):
      rem = init
      for byte in msg:
        rem = self.CRC8Table[ (rem ^ byte)] & 0xff
      return rem
    # ----------------------------------------------------

    def connect(self, port, baudrate=115200):
        if self.ser and self.ser.is_open:
            if self.port == port: return True
            self.disconnect()
        try:
            self.port = port
            self.ser = serial.Serial(self.port, baudrate, timeout=1)
            print(f"Successfully connected to {self.port}")
            self._start_reading()
            return True
        except serial.SerialException as e:
            print(f"Error connecting to {self.port}: {e}")
            self.ser = None; self.port = None
            return False

    def disconnect(self):
        self._stop_reading()
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"Disconnected from {self.port}")
        self.ser = None; self.port = None

    def _start_reading(self):
        if self.read_thread is None:
            self._running = True
            self.read_thread = threading.Thread(target=self._read_loop)
            self.read_thread.daemon = True
            self.read_thread.start()

    def _stop_reading(self):
        self._running = False
        if self.read_thread and self.read_thread.is_alive():
            self.read_thread.join()
        self.read_thread = None

    def _read_loop(self):
        """Continuously reads from the serial port and puts lines in a queue."""
        while self._running and self.ser and self.ser.is_open:
            try:
                # Use readline() to properly capture lines of debug text from firmware
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    self.rx_queue.append(line)
            except serial.SerialException:
                print("Serial port disconnected during read.")
                break
            except Exception as e:
                print(f"ERROR: Serial read loop exception: {e}")
            # No sleep needed as readline() is blocking with a timeout

    def get_all_received_lines(self):
        """Pops all current lines from the queue and returns them."""
        lines = []
        while True:
            try:
                lines.append(self.rx_queue.popleft())
            except IndexError:
                break
        return lines

    def _create_packet_v2(self, state):
        buttons = state.get('buttons', 0); dpad = state.get('dpad', 0)
        lx = max(-127, min(127, state.get('lx', 0))); ly = max(-127, min(127, state.get('ly', 0)))
        rx = max(-127, min(127, state.get('rx', 0))); ry = max(-127, min(127, state.get('ry', 0)))
        l2 = max(0, min(255, state.get('l2', 0))); r2 = max(0, min(255, state.get('r2', 0)))
        accel_x = max(-32767, min(32767, state.get('accel_x', 0)))
        accel_y = max(-32767, min(32767, state.get('accel_y', 0)))
        accel_z = max(-32767, min(32767, state.get('accel_z', 0)))
        gyro_x = max(-32767, min(32767, state.get('gyro_x', 0)))
        gyro_y = max(-32767, min(32767, state.get('gyro_y', 0)))
        gyro_z = max(-32767, min(32767, state.get('gyro_z', 0)))

        header = 0xA6

        payload = struct.pack('<HbbBBbbB6h',
            buttons, lx, ly, l2, r2, rx, ry, dpad,
            accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z
        )

        message_to_checksum = bytearray([header]) + payload

        # Calculate CRC8 checksum instead of simple XOR
        checksum = self._crc8(message_to_checksum, 0xFF)

        return bytearray([header]) + payload + bytearray([checksum])

    def send_gamepad_state_v2(self, state):
        if not self.ser or not self.ser.is_open: return
        packet = self._create_packet_v2(state)
        self.ser.write(packet)

    def send_rf_code(self, code):
        """Creates and sends a packet to command the firmware to send an RF code."""
        if not self.ser or not self.ser.is_open:
            print("ERROR: Serial port not connected. Cannot send RF code.")
            return

        header = 0xA7
        # Pack the code as a 4-byte unsigned long, little-endian
        payload = struct.pack('<L', code)

        message_to_checksum = bytearray([header]) + payload
        checksum = self._crc8(message_to_checksum, 0xFF)

        packet = bytearray([header]) + payload + bytearray([checksum])

        print(f"DEBUG: Sending RF code {code} with packet: {packet.hex()}")
        self.ser.write(packet)
