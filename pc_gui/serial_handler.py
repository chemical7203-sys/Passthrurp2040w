import serial
import struct
import time
import threading
from PyQt6.QtCore import QObject, pyqtSignal

class SerialSignals(QObject):
    received_line = pyqtSignal(str)

class SerialHandler:
    def __init__(self):
        self.ser = None
        self.port = None
        self.signals = SerialSignals()
        self._running = False
        self.read_thread = None

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
        self._running = True
        self.read_thread = threading.Thread(target=self._read_loop)
        self.read_thread.daemon = True
        self.read_thread.start()

    def _stop_reading(self):
        self._running = False
        if self.read_thread and self.read_thread.is_alive():
            self.read_thread.join()

    def _read_loop(self):
        while self._running and self.ser and self.ser.is_open:
            try:
                if self.ser.in_waiting > 0:
                    line = self.ser.readline().decode('utf-8').strip()
                    if line:
                        self.signals.received_line.emit(line)
            except serial.SerialException:
                print("Serial port disconnected during read.")
                break
            except Exception as e:
                print(f"Error in read loop: {e}")
            time.sleep(0.01)

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
