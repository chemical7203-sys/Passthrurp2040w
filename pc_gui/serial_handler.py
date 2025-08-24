import serial
import struct
import time

class SerialHandler:
    def __init__(self):
        self.ser = None
        self.port = None

    def connect(self, port, baudrate=115200):
        if self.ser and self.ser.is_open:
            if self.port == port: return True
            self.disconnect()
        try:
            self.port = port
            self.ser = serial.Serial(self.port, baudrate, timeout=0.05) # Non-blocking
            print(f"Successfully connected to {self.port}")
            return True
        except serial.SerialException as e:
            print(f"Error connecting to {self.port}: {e}")
            self.ser = None; self.port = None
            return False

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"Disconnected from {self.port}")
        self.ser = None; self.port = None

    def read_line(self):
        """Reads a line from the serial port if available."""
        if not self.ser or not self.ser.is_open or self.ser.in_waiting == 0:
            return None
        try:
            line = self.ser.readline().decode('utf-8').strip()
            return line
        except Exception:
            return None

    def _create_packet_v2(self, state):
        buttons = state.get('buttons', 0); dpad = state.get('dpad', 0)
        lx = max(-127, min(127, state.get('lx', 0))); ly = max(-127, min(127, state.get('ly', 0)))
        rx = max(-127, min(127, state.get('rx', 0))); ry = max(-127, min(127, state.get('ry', 0)))
        l2 = max(0, min(255, state.get('l2', 0))); r2 = max(0, min(255, state.get('r2', 0)))
        header = 0xA6
        packet_data = struct.pack('<H4b2B', buttons, lx, ly, rx, ry, l2, r2)
        checksum = header
        for byte in packet_data: checksum ^= byte
        checksum ^= dpad
        return bytearray([header]) + packet_data + bytearray([dpad, checksum])

    def send_gamepad_state_v2(self, state):
        if not self.ser or not self.ser.is_open: return
        packet = self._create_packet_v2(state)
        self.ser.write(packet)
