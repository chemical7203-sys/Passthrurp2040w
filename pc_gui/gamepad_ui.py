import sys
from PyQt6.QtWidgets import (
    QApplication, QWidget, QLabel, QVBoxLayout, QGridLayout, QProgressBar,
    QLineEdit, QPushButton, QHBoxLayout
)
from PyQt6.QtGui import QFont, QColor
from PyQt6.QtCore import Qt, QObject, pyqtSignal

# A separate class for defining PyQt signals.
# This helps keep the code organized.
class GamepadSignals(QObject):
    stick_event = pyqtSignal(str, int)      # e.g., ('ABS_X', 128)
    trigger_event = pyqtSignal(str, int)    # e.g., ('ABS_Z', 255)
    button_event = pyqtSignal(str, bool)    # e.g., ('BTN_SOUTH', True)
    dpad_event = pyqtSignal(str, int)        # e.g., ('ABS_HAT0X', 1)


class GamepadUI(QWidget):
    """This class sets up the UI layout and provides slots to update it."""
    def __init__(self):
        super().__init__()
        self.initUI()

        # A map to easily find button labels by their code
        self.button_labels = {
            'BTN_SOUTH': self.btn_south_label,
            'BTN_EAST': self.btn_east_label
        }

    def initUI(self):
        self.setWindowTitle('RP2040 Gamepad Passthrough')
        self.setGeometry(100, 100, 450, 350)

        main_layout = QVBoxLayout()
        self.setLayout(main_layout)

        title = QLabel('DS4 Controller Status')
        title.setFont(QFont('Arial', 18, QFont.Weight.Bold))
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        main_layout.addWidget(title)

        grid = QGridLayout()
        main_layout.addLayout(grid)

        # --- UI Elements ---
        grid.addWidget(self._create_group_label('Left Stick'), 0, 0)
        self.ls_x_label = self._create_value_label('X: 128')
        self.ls_y_label = self._create_value_label('Y: 128')
        grid.addWidget(self.ls_x_label, 1, 0)
        grid.addWidget(self.ls_y_label, 1, 1)

        grid.addWidget(self._create_group_label('Buttons'), 2, 0)
        self.btn_south_label = self._create_status_label('South (A/X)')
        self.btn_east_label = self._create_status_label('East (B/O)')
        grid.addWidget(self.btn_south_label, 3, 0)
        grid.addWidget(self.btn_east_label, 3, 1)

        grid.addWidget(self._create_group_label('Triggers'), 4, 0)
        self.l2_trigger_bar = QProgressBar(maximum=255)
        self.r2_trigger_bar = QProgressBar(maximum=255)
        grid.addWidget(QLabel("L2"), 5, 0)
        grid.addWidget(self.l2_trigger_bar, 5, 1)
        grid.addWidget(QLabel("R2"), 6, 0)
        grid.addWidget(self.r2_trigger_bar, 6, 1)

        main_layout.addStretch(1)

        # --- Serial Connection UI ---
        main_layout.addWidget(self._create_group_label('Connection'))
        conn_layout = QHBoxLayout()
        self.port_input = QLineEdit("COM3") # Default for Windows
        self.connect_button = QPushButton("Connect")
        conn_layout.addWidget(QLabel("Serial Port:"))
        conn_layout.addWidget(self.port_input)
        conn_layout.addWidget(self.connect_button)
        main_layout.addLayout(conn_layout)

    # --- Helper methods for creating widgets ---
    def _create_group_label(self, text):
        label = QLabel(text)
        label.setFont(QFont('Arial', 12, QFont.Weight.Bold))
        label.setStyleSheet("margin-top: 10px; margin-bottom: 5px;")
        return label

    def _create_value_label(self, text):
        label = QLabel(text)
        label.setFont(QFont('Arial', 10))
        return label

    def _create_status_label(self, text):
        label = QLabel(text)
        label.setFont(QFont('Arial', 10))
        label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        label.setStyleSheet("border: 1px solid grey; padding: 5px; background-color: lightgrey; border-radius: 5px;")
        return label

    # --- SLOTS to update the UI based on signals ---
    def update_stick(self, code, value):
        if code == 'ABS_X':
            self.ls_x_label.setText(f"X: {value}")
        elif code == 'ABS_Y':
            self.ls_y_label.setText(f"Y: {value}")
        # Add right stick later if needed

    def update_trigger(self, code, value):
        if code == 'ABS_Z': # L2
            self.l2_trigger_bar.setValue(value)
        elif code == 'ABS_RZ': # R2
            self.r2_trigger_bar.setValue(value)

    def update_button(self, code, pressed):
        if code in self.button_labels:
            label = self.button_labels[code]
            if pressed:
                label.setStyleSheet("border: 2px solid blue; padding: 5px; background-color: lightblue; border-radius: 5px;")
            else:
                label.setStyleSheet("border: 1px solid grey; padding: 5px; background-color: lightgrey; border-radius: 5px;")

    def update_dpad(self, code, value):
        # We can add a visual for the DPad later
        pass

# Standalone testing block
if __name__ == '__main__':
    app = QApplication(sys.argv)
    ui = GamepadUI()
    ui.show()

    # --- Example of how to test the slots ---
    print("Testing UI slots...")
    ui.update_stick('ABS_X', 255)
    ui.update_trigger('ABS_RZ', 150)
    ui.update_button('BTN_SOUTH', True)

    sys.exit(app.exec())
