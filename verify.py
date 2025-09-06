import serial
import time
import sys

# --- Configuration ---
# Please change this to your Pico's serial port.
# Examples:
# Windows: 'COM3'
# Linux:   '/dev/ttyACM0'
# macOS:   '/dev/cu.usbmodem1411'
SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200

def main():
    """Main function to run the verification interactive prompt."""
    print(f"--- Raspberry Pi Pico CC1101 Cloner Verification Script ---")
    print(f"Attempting to connect to Pico on {SERIAL_PORT} at {BAUD_RATE} bps...")

    try:
        # The timeout is crucial to prevent readline() from blocking indefinitely.
        pico = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
    except serial.SerialException as e:
        print(f"\n[ERROR] Could not open serial port '{SERIAL_PORT}'.")
        print(f"Details: {e}")
        print("Please check the following:")
        print("1. Is the Pico connected to your computer?")
        print("2. Is the SERIAL_PORT variable in this script set correctly?")
        print("3. Do you have the necessary permissions to access the port?")
        sys.exit(1)

    print("Connection successful. Waiting for the device to be ready...")

    # Wait for the initial "STATUS:Ready" message from the Pico to ensure sync.
    if not wait_for_status(pico, "Ready", timeout_seconds=5):
        print("\n[ERROR] Device did not become ready. Please reset the Pico and try again.")
        pico.close()
        sys.exit(1)

    print("\n--- Pico is Ready ---")

    while True:
        print("\nSelect an action:")
        print("  [c] Capture a new 433MHz signal")
        print("  [t] Transmit the last captured signal")
        print("  [q] Quit")
        choice = input("Enter your choice: ").strip().lower()

        if choice == 'c':
            capture_signal(pico)
        elif choice == 't':
            transmit_signal(pico)
        elif choice == 'q':
            break
        else:
            print("Invalid choice. Please try again.")

    print("Closing serial port. Goodbye!")
    pico.close()

def wait_for_status(pico_serial, expected_status, timeout_seconds=10):
    """
    Reads lines from the serial port until a specific status is found or a timeout occurs.
    Returns True if the status is found, False otherwise.
    """
    print(f"Waiting for status: '{expected_status}'...")
    start_time = time.time()
    while time.time() - start_time < timeout_seconds:
        line = pico_serial.readline().decode('utf-8').strip()
        if line:
            print(f"PICO > {line}")
            if f"STATUS:{expected_status}" in line:
                return True
    return False

def capture_signal(pico_serial):
    """Sends the 'c' command and handles the data capture process."""
    print("\n--- Initiating Capture ---")
    pico_serial.write(b'c')

    if not wait_for_status(pico_serial, "Armed"):
        print("[ERROR] Device did not arm for capture.")
        return

    print("\nDevice is armed. Please press and hold a button on your 433MHz remote.")
    print("Waiting to receive data from Pico...")

    while True:
        line = pico_serial.readline().decode('utf-8').strip()
        if not line:
            continue # Ignore empty lines from timeout

        print(f"PICO > {line}")
        if line.startswith("DATA:"):
            data_str = line.replace("DATA:", "")
            try:
                pulses = [int(p) for p in data_str.split(',')]
                print(f"\n[SUCCESS] Captured {len(pulses)} pulses.")
                print(f"  First 15 pulses (us): {pulses[:15]}")
                # The device should automatically report it's ready again.
                wait_for_status(pico_serial, "Ready")
                return
            except ValueError:
                print("[ERROR] Could not parse data received from Pico.")
                return
        elif "STATUS:Ready" in line:
            print("[INFO] Capture timed out or was aborted on the Pico. Ready for new command.")
            return


def transmit_signal(pico_serial):
    """Sends the 't' command to transmit the last captured signal."""
    print("\n--- Initiating Transmission ---")
    pico_serial.write(b't')

    # Wait for both "Transmit complete" and "Ready" to confirm the full cycle.
    if wait_for_status(pico_serial, "Transmit complete"):
        wait_for_status(pico_serial, "Ready")
        print("[SUCCESS] Transmission cycle complete.")
    else:
        print("[ERROR] Did not receive transmit confirmation. The Pico may not have a signal stored.")
        # Check if it just reported an error and went back to ready.
        wait_for_status(pico_serial, "Ready")


if __name__ == "__main__":
    print("NOTE: Please ensure you have pyserial installed (`pip install pyserial`)")
    main()
