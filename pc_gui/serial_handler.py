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
        # Use a deque for thread-safe, efficient appends and pops
        self.rx_queue = deque(maxlen=100)

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
                # Low-level debug to see if any bytes are coming in at all
                if self.ser.in_waiting > 0:
                    raw_bytes = self.ser.read(self.ser.in_waiting)
                    print(f"RAW BYTES RECEIVED: {' '.join(f'{b:02x}' for b in raw_bytes)}")
                    # The original logic remains, but we now see the raw data first
                    # It's likely the readline below will not work if no newline is sent
                    line = raw_bytes.decode('utf-8', errors='ignore').strip()
                    if line:
                        self.rx_queue.append(line)

            except serial.SerialException:
                print("Serial port disconnected during read.")
                break
            except Exception as e:
                print(f"ERROR: Serial read loop exception: {e}")
            time.sleep(0.01) # Yield CPU

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

        dummy_start = 0x00
        dummy_end = 0x00
        header = 0xA6

        # 1. Construct the 11-byte padded payload
        payload = struct.pack('<BH4b2BBB', dummy_start, buttons, lx, ly, rx, ry, l2, r2, dpad, dummy_end)

        # 2. Calculate checksum over the header and the 11-byte padded payload
        checksum = header
        for byte in payload:
            checksum ^= byte

        # 3. Return the final 13-byte packet
        return bytearray([header]) + payload + bytearray([checksum])

    def send_gamepad_state_v2(self, state):
        if not self.ser or not self.ser.is_open: return
        packet = self._create_packet_v2(state)
        self.ser.write(packet)
