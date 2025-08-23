import pygame
from serial.tools import list_ports

def get_available_gamepads():
    """Returns a list of available gamepad devices using pygame."""
    pygame.init()
    pygame.joystick.init()

    gamepads = []
    for i in range(pygame.joystick.get_count()):
        joystick = pygame.joystick.Joystick(i)
        joystick.init()
        gamepads.append({
            "index": i,
            "name": joystick.get_name(),
        })

    # It's good practice to quit the joystick subsystem after scanning
    pygame.joystick.quit()
    pygame.quit()

    if not gamepads:
        print("No gamepads found by pygame.")
    return gamepads

def get_available_serial_ports():
    """Returns a list of available serial ports."""
    ports = list_ports.comports()
    if not ports:
        print("No serial ports found.")
    return ports

# This block allows for standalone testing of this module
if __name__ == '__main__':
    print("--- Testing Device Manager with Pygame ---")

    print("\n--- Available Gamepads ---")
    gamepad_list = get_available_gamepads()
    if gamepad_list:
        for gamepad in gamepad_list:
            print(f"  Index {gamepad['index']}: {gamepad['name']}")

    print("\n--- Available Serial Ports ---")
    port_list = get_available_serial_ports()
    if port_list:
        for port in port_list:
            print(f"  - Device: {port.device}")
            print(f"    Description: {port.description}")
            print(f"    HWID: {port.hwid}")
