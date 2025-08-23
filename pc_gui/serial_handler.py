import serial
import struct
import time

class SerialHandler:
    """Handles the connection and data transmission over the serial port."""
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
        """Creates and sends a gamepad state packet."""
        if not self.ser or not self.ser.is_open:
            # print("Warning: Serial port not connected. Cannot send data.")
            return

        packet = self._create_packet(button_state, joy_x, joy_y)
        self.ser.write(packet)
        # For debugging:
        # print(f"Sent Packet: {packet.hex(' ')}")
