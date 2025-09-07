import serial
import time
import sys

def list_ports():
    """
    Lists serial port names
    """
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()
    print("Available serial ports:")
    for port, desc, hwid in sorted(ports):
        print(f"- {port}: {desc} [{hwid}]")
    return [p.device for p in ports]

def listen_for_device_output(ser, duration_s=1):
    """Reads and prints all incoming lines from the device for a short duration."""
    start_time = time.time()
    while time.time() - start_time < duration_s:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"DEV: {line}")
        time.sleep(0.01)

def main():
    """
    Main function to run the control script.
    """
    print("--- RP2040 CC1101 Signal Cloner Control ---")

    available_ports = list_ports()
    if not available_ports:
        print("\nNo serial ports found. Please ensure your device is connected.")
        return

    port = input(f"\nEnter the serial port name (e.g., {available_ports[0]}): ")

    if port not in available_ports:
        print(f"Error: Port '{port}' not found.")
        return

    try:
        ser = serial.Serial(port, 115200, timeout=1)
        print(f"Connected to {port}.")
    except serial.SerialException as e:
        print(f"Error connecting to port: {e}")
        return

    # Wait for the initial messages from the Pico and print them
    listen_for_device_output(ser, 2.5)

    while True:
        choice = input("\nChoose an action: (c)apture, (t)ransmit, (q)uit: ").lower()

        if choice == 'q':
            print("Exiting.")
            ser.close()
            sys.exit(0)

        elif choice == 'c':
            ser.write(b'c')
            print("Sent 'c' command. Waiting for response...")

            capturing = False
            data_line_found = False

            while not data_line_found:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if not line:
                        print("Timeout waiting for data.")
                        break

                    print(f"DEV: {line}")

                    if "Capture finished" in line:
                        capturing = True
                        continue

                    if capturing:
                        pulses_str = line.split(',')
                        pulses_us = [float(p) for p in pulses_str if p]
                        print("\n--- Captured Pulse Data (µs) ---")
                        print(f"Number of pulses: {len(pulses_us)}")
                        print(line) # Print the raw comma-separated string
                        print("---------------------------------")
                        data_line_found = True

                except (UnicodeDecodeError, KeyboardInterrupt):
                    print("\nExiting.")
                    ser.close()
                    return

        elif choice == 't':
            print("\nEnter the comma-separated pulse durations (in µs) to transmit.")
            data_to_send = input("> ")

            if not data_to_send:
                print("No data entered. Aborting transmit.")
                continue

            print("Sending 't' command and data...")
            ser.write(b't')
            # The firmware expects the data on a new line
            ser.write(f"{data_to_send}\n".encode('utf-8'))

            print("Data sent. Listening for confirmation...")
            listen_for_device_output(ser, 3) # Listen for a few seconds for the response

        else:
            print("Invalid choice. Please try again.")


if __name__ == "__main__":
    main()
