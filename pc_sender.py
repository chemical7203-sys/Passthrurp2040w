import serial
import time
import struct

# --- Configuration ---
#
# !!! IMPORTANT !!!
# This has been adapted for the sandboxed environment.
#
SERIAL_PORT = '/dev/ttyS1' # Using ttyS1 as a guess for uart1
BAUD_RATE = 115200

# --- Protocol Definitions ---
PROTOCOL_HEADER = 0xA6

def create_packet_v2(buttons, lx, ly, l2, r2, rx, ry, dpad):
    """Creates a 23-byte packet compatible with the firmware's gamepad_data_v2_t."""

    # For now, accel and gyro data are zeroed out as we are focusing on basic inputs.
    accel_x, accel_y, accel_z = 0, 0, 0
    gyro_x, gyro_y, gyro_z = 0, 0, 0

    # Pack the data according to the gamepad_data_v2_t struct format
    # Format: <H b b B B b b B h h h h h h
    # H: buttons (uint16)
    # b: lx, ly, rx, ry (int8)
    # B: l2, r2, dpad (uint8)
    # h: accel/gyro (int16)
    payload = struct.pack('<HbbBBbbBhhhhhh',
                          buttons, lx, ly, l2, r2, rx, ry, dpad,
                          accel_x, accel_y, accel_z,
                          gyro_x, gyro_y, gyro_z)

    # Prepare the full message with header
    message = bytearray([PROTOCOL_HEADER]) + payload

    # Calculate checksum over the header and payload
    checksum = 0
    for byte in message:
        checksum ^= byte

    # Append checksum to create the final packet
    packet = message + bytearray([checksum])

    return packet

def main():
    """Main function to connect to the serial port and send test packets."""
    print("=========================================")
    print("=== Pico Gamepad UART Test Sender (v2) ===")
    print("=========================================")
    print(f"Attempting to connect to port '{SERIAL_PORT}' at {BAUD_RATE} baud.")

    try:
        # Open the serial port
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print("Serial port opened successfully.")
        print("Will now send test packets every 200ms. Press Ctrl+C to exit.")
        print("-" * 41)
    except serial.SerialException as e:
        print(f"\n[ERROR] Could not open serial port '{SERIAL_PORT}'.")
        print(f"  > {e}")
        print("\nPlease check the following:")
        print("  1. Is the firmware running and providing a virtual serial port?")
        print("  2. Is the SERIAL_PORT variable in this script set correctly?")
        print("  3. Do you have the necessary permissions to access the port?")
        return

    # A list of different inputs to test
    # Values correspond to a real DS4 controller input
    test_cases = [
        {"name": "Center", "buttons": 0, "lx": 0, "ly": 0, "l2": 0, "r2": 0, "rx": 0, "ry": 0, "dpad": 8},
        {"name": "Right", "buttons": 0, "lx": 127, "ly": 0, "l2": 0, "r2": 0, "rx": 0, "ry": 0, "dpad": 8},
        {"name": "Left", "buttons": 0, "lx": -127, "ly": 0, "l2": 0, "r2": 0, "rx": 0, "ry": 0, "dpad": 8},
        {"name": "D-Pad Up", "buttons": 0, "lx": 0, "ly": 0, "l2": 0, "r2": 0, "rx": 0, "ry": 0, "dpad": 0},
        {"name": "Cross", "buttons": (1<<1), "lx": 0, "ly": 0, "l2": 0, "r2": 0, "rx": 0, "ry": 0, "dpad": 8},
        {"name": "L2 Trigger", "buttons": 0, "lx": 0, "ly": 0, "l2": 255, "r2": 0, "rx": 0, "ry": 0, "dpad": 8},
    ]

    case_index = 0
    try:
        while True:
            # Get the current test case
            case = test_cases[case_index]

            # Create the packet
            packet = create_packet_v2(
                case["buttons"], case["lx"], case["ly"], case["l2"], case["r2"],
                case["rx"], case["ry"], case["dpad"]
            )

            # Print and send
            print(f"Sending ({case['name']}): Packet = {packet.hex(' ')}")
            ser.write(packet)

            # Wait
            time.sleep(0.2)

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
