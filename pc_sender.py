import serial
import time
import struct

# --- Configuration ---
#
# !!! IMPORTANT !!!
# Please change this to the serial port of your USB-to-UART adapter.
# - On Windows, it will be something like 'COM3', 'COM4', etc.
# - On macOS, it will be something like '/dev/cu.usbserial-XXXX'
# - On Linux, it will be something like '/dev/ttyUSB0' or '/dev/ttyACM0'
#
SERIAL_PORT = 'COM3'  # <-- CHANGE THIS!
BAUD_RATE = 115200

# --- Protocol Definitions ---
PROTOCOL_HEADER = 0xA5

def create_packet(button_state, joy_x, joy_y):
    """Creates a 5-byte packet according to the defined protocol."""
    # Ensure joystick values are within the valid signed 8-bit range
    joy_x = max(-127, min(127, joy_x))
    joy_y = max(-127, min(127, joy_y))

    # In Python, to get the correct byte value for a negative number for XOR,
    # we can use struct.pack to convert it to a signed byte and then unpack it.
    # This correctly handles two's complement representation.
    byte_joy_x = struct.pack('b', joy_x)[0]
    byte_joy_y = struct.pack('b', joy_y)[0]

    # Calculate checksum using XOR on the byte values
    checksum = PROTOCOL_HEADER ^ button_state ^ byte_joy_x ^ byte_joy_y

    # Construct the final packet
    packet = bytearray([
        PROTOCOL_HEADER,
        button_state,
        byte_joy_x,
        byte_joy_y,
        checksum
    ])
    return packet

def main():
    """Main function to connect to the serial port and send test packets."""
    print("=========================================")
    print("=== Pico Gamepad UART Test Sender ===")
    print("=========================================")
    print(f"Attempting to connect to port '{SERIAL_PORT}' at {BAUD_RATE} baud.")

    try:
        # Open the serial port
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print("Serial port opened successfully.")
        print("Will now send test packets every second. Press Ctrl+C to exit.")
        print("-" * 41)
    except serial.SerialException as e:
        print(f"\n[ERROR] Could not open serial port '{SERIAL_PORT}'.")
        print(f"  > {e}")
        print("\nPlease check the following:")
        print("  1. Is your RP2040 connected to the PC via the USB-to-UART adapter?")
        print("  2. Is the SERIAL_PORT variable in this script set correctly?")
        print("  3. Do you have the necessary permissions to access the port?")
        return

    # A list of different inputs to test
    test_cases = [
        {"name": "Center",          "buttons": 0x00, "x": 0, "y": 0},
        {"name": "Right + Btn 1",   "buttons": 0x01, "x": 127, "y": 0},
        {"name": "Left",            "buttons": 0x00, "x": -127, "y": 0},
        {"name": "Up",              "buttons": 0x00, "x": 0, "y": 127},
        {"name": "Down + Btn 1",    "buttons": 0x01, "x": 0, "y": -127},
        {"name": "Top-Right",       "buttons": 0x00, "x": 90, "y": 90},
        {"name": "Bottom-Left",     "buttons": 0x00, "x": -90, "y": -90},
    ]

    case_index = 0
    try:
        while True:
            # Get the current test case
            case = test_cases[case_index]
            buttons, x, y = case["buttons"], case["x"], case["y"]

            # Create the packet
            packet = create_packet(buttons, x, y)

            # Print and send
            print(f"Sending ({case['name']}): Packet = {packet.hex(' ')}")
            ser.write(packet)

            # Wait for 1 second
            time.sleep(1)

            # Move to the next test case
            case_index = (case_index + 1) % len(test_cases)

    except KeyboardInterrupt:
        print("\n\nProgram terminated by user.")
    except Exception as e:
        print(f"\n[ERROR] An unexpected error occurred: {e}")
    finally:
        # Ensure the serial port is closed on exit
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Serial port closed.")

if __name__ == "__main__":
    main()
