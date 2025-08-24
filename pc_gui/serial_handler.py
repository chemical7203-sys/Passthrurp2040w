import serial
import struct
import time

class SerialHandler:
    """
    Handles the connection and data transmission over the serial port.

    Protocol v2 (Full Gamepad): 11-byte packet
    - Byte 0:  Header (0xA6)
    - Byte 1:  Buttons (LSB)
    - Byte 2:  Buttons (MSB)
    - Byte 3:  Left Stick X   (int8)
    - Byte 4:  Left Stick Y   (int8)
    - Byte 5:  Right Stick X  (int8)
    - Byte 6:  Right Stick Y  (int8)
    - Byte 7:  L2 Trigger     (uint8)
    - Byte 8:  R2 Trigger     (uint8)
    - Byte 9:  D-Pad          (bitmask)
    - Byte 10: Checksum
    """
    def __init__(self):
        self.ser = None
        self.port = None

    def connect(self, port, baudrate=115200):
        """Tries to connect to the specified serial port."""
        if self.ser and self.ser.is_open:
            if self.port == port:
                return True # Already connected to the same port
            self.disconnect()

        try:
            self.port = port
            self.ser = serial.Serial(self.port, baudrate, timeout=1)
            print(f"Successfully connected to {self.port}")
            return True
        except serial.SerialException as e:
            print(f"Error connecting to {self.port}: {e}")
            self.ser = None
            self.port = None
            return False

    def disconnect(self):
        """Closes the serial connection if it's open."""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"Disconnected from {self.port}")
        self.ser = None
        self.port = None

    def _create_packet(self, button_state, joy_x, joy_y):
        """
        Creates a 5-byte packet according to the v1 protocol.
        - joy_x and joy_y are expected to be in the range -127 to 127.
        - button_state is a bitmask (0-255).
        """
        # Ensure values are within the valid range
        joy_x = max(-127, min(127, joy_x))
        joy_y = max(-127, min(127, joy_y))

        # Pack the joystick values as signed 8-bit integers
        byte_joy_x = struct.pack('b', joy_x)[0]
        byte_joy_y = struct.pack('b', joy_y)[0]

        header = 0xA5
        checksum = header ^ button_state ^ byte_joy_x ^ byte_joy_y

        packet = bytearray([header, button_state, byte_joy_x, byte_joy_y, checksum])
        return packet

    def send_gamepad_state(self, button_state, joy_x, joy_y):
        """Creates and sends a v1 gamepad state packet."""
        if not self.ser or not self.ser.is_open:
            return

        packet = self._create_packet(button_state, joy_x, joy_y)
        self.ser.write(packet)

    def _create_packet_v2(self, state):
        """Creates an 11-byte packet according to the v2 protocol."""

        # Pack the values, ensuring they are within the correct range
        buttons = state.get('buttons', 0)
        lx = max(-127, min(127, state.get('lx', 0)))
        ly = max(-127, min(127, state.get('ly', 0)))
        rx = max(-127, min(127, state.get('rx', 0)))
        ry = max(-127, min(127, state.get('ry', 0)))
        l2 = max(0, min(255, state.get('l2', 0)))
        r2 = max(0, min(255, state.get('r2', 0)))
        dpad = state.get('dpad', 0)

        header = 0xA6

        # Use struct to pack the data consistently
        # B = uint8, b = int8, H = uint16 (little-endian by default)
        # < for little-endian
        packet_data = struct.pack('<H4b2B', buttons, lx, ly, rx, ry, l2, r2)

        # Checksum calculation
        checksum = header
        for byte in packet_data:
            checksum ^= byte
        checksum ^= dpad

        # Final packet
        packet = bytearray([header]) + packet_data + bytearray([dpad, checksum])
        return packet

    def send_gamepad_state_v2(self, state):
        """Creates and sends a v2 gamepad state packet."""
        if not self.ser or not self.ser.is_open:
            return

        packet = self._create_packet_v2(state)
        self.ser.write(packet)
        # For debugging:
        # print(f"Sent V2 Packet: {packet.hex(' ')}")
