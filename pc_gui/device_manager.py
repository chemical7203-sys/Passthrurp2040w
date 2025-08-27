import serial.tools.list_ports

def get_available_serial_ports():
    """Returns a list of available serial ports."""
    ports = serial.tools.list_ports.comports()
    return ports
