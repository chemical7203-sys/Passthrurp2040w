from inputs import devices
from serial.tools import list_ports

def get_available_gamepads():
    """Returns a list of available gamepad devices."""
    gamepads = devices.gamepads
    if not gamepads:
        print("No gamepads found.")
    return gamepads

def get_available_serial_ports():
    """Returns a list of available serial ports."""
    ports = list_ports.comports()
    if not ports:
        print("No serial ports found.")
    return ports

# This block allows for standalone testing of this module
if __name__ == '__main__':
    print("--- Available Gamepads ---")
    gamepad_list = get_available_gamepads()
    if gamepad_list:
        for i, gamepad in enumerate(gamepad_list):
            print(f"  {i}: {gamepad}")

    print("\n--- Available Serial Ports ---")
    port_list = get_available_serial_ports()
    if port_list:
        for port in port_list:
            print(f"  - Device: {port.device}")
            print(f"    Description: {port.description}")
            print(f"    HWID: {port.hwid}")
