import sys
from PyQt6.QtWidgets import (
    QApplication, QWidget, QLabel, QVBoxLayout, QGridLayout, QComboBox,
    QPushButton, QHBoxLayout, QGraphicsView, QGraphicsScene, QGraphicsEllipseItem,
    QGraphicsRectItem, QTextEdit
)
from PyQt6.QtGui import QFont, QColor, QBrush, QPen
from PyQt6.QtCore import Qt, QObject, pyqtSignal

# --- Signals ---
class GamepadSignals(QObject):
    stick_event = pyqtSignal(str, float)
    trigger_event = pyqtSignal(str, float)
    button_event = pyqtSignal(str, bool)
    dpad_event = pyqtSignal(str, int)
    gamepad_disconnected = pyqtSignal()
    raw_event = pyqtSignal(str)

# --- Gamepad Graphics Widget ---
class GamepadWidget(QGraphicsView):
    def __init__(self):
        super().__init__()
        self.scene = QGraphicsScene(self)
        self.setScene(self.scene)
        self.setFixedSize(450, 250)

        self.stick_x_val, self.stick_y_val = 0.0, 0.0
        self.rstick_x_val, self.rstick_y_val = 0.0, 0.0

        self._draw_layout()

    def _draw_layout(self):
        # Colors and pens
        body_brush = QBrush(QColor("#cccccc"))
        stick_bg_brush = QBrush(QColor("#bbbbbb"))
        self.stick_brush = QBrush(QColor("#666666"))
        self.btn_off_brush = QBrush(QColor("#aaaaaa"))
        self.btn_on_brush = QBrush(QColor("#3399ff"))
        self.dpad_brush = QBrush(QColor("#888888"))

        # Body
        self.scene.addRect(0, 25, 450, 150, QPen(Qt.GlobalColor.transparent), body_brush)

        # D-Pad
        self.dpad_up = self.scene.addRect(55, 60, 20, 25, QPen(Qt.GlobalColor.black), self.dpad_brush)
        self.dpad_down = self.scene.addRect(55, 115, 20, 25, QPen(Qt.GlobalColor.black), self.dpad_brush)
        self.dpad_left = self.scene.addRect(30, 85, 25, 20, QPen(Qt.GlobalColor.black), self.dpad_brush)
        self.dpad_right = self.scene.addRect(75, 85, 25, 20, QPen(Qt.GlobalColor.black), self.dpad_brush)

        # Sticks
        self.scene.addEllipse(120, 90, 80, 80, QPen(Qt.GlobalColor.transparent), stick_bg_brush)
        self.left_stick = self._create_stick_item(160, 130)

        self.scene.addEllipse(250, 90, 80, 80, QPen(Qt.GlobalColor.transparent), stick_bg_brush)
        self.right_stick = self._create_stick_item(290, 130)

        # Buttons
        self.buttons = {
            'BTN_WEST': self._create_button_item(350, 95),  # Square
            'BTN_NORTH': self._create_button_item(380, 65), # Triangle
            'BTN_EAST': self._create_button_item(410, 95),  # Circle
            'BTN_SOUTH': self._create_button_item(380, 125), # X
        }

        # Triggers and Shoulders
        self.l1_btn = self.scene.addRect(30, 25, 80, 20, QPen(Qt.GlobalColor.black), self.btn_off_brush)
        self.r1_btn = self.scene.addRect(340, 25, 80, 20, QPen(Qt.GlobalColor.black), self.btn_off_brush)
        self.l2_label = self.scene.addText("L2: 0", QFont("Arial", 10))
        self.l2_label.setPos(30, 0)
        self.r2_label = self.scene.addText("R2: 0", QFont("Arial", 10))
        self.r2_label.setPos(340, 0)
        self.buttons.update({'BTN_TL': self.l1_btn, 'BTN_TR': self.r1_btn})

    def _create_stick_item(self, x, y):
        stick = QGraphicsEllipseItem(-15, -15, 30, 30)
        stick.setBrush(self.stick_brush)
        stick.setPos(x, y)
        self.scene.addItem(stick)
        return stick

    def _create_button_item(self, x, y):
        button = QGraphicsEllipseItem(-10, -10, 20, 20)
        button.setBrush(self.btn_off_brush)
        button.setPos(x, y)
        self.scene.addItem(button)
        return button

    # --- SLOTS ---
    def update_stick(self, code, value):
        if code == 'ABS_X': self.stick_x_val = value
        elif code == 'ABS_Y': self.stick_y_val = value
        elif code == 'ABS_RX': self.rstick_x_val = value
        elif code == 'ABS_RY': self.rstick_y_val = value
        self.left_stick.setPos(160 + (self.stick_x_val * 30), 130 + (self.stick_y_val * 30))
        self.right_stick.setPos(290 + (self.rstick_x_val * 30), 130 + (self.rstick_y_val * 30))

    def update_button(self, code, pressed):
        if code in self.buttons:
            self.buttons[code].setBrush(self.btn_on_brush if pressed else self.btn_off_brush)

    def update_dpad(self, code, value):
        if code == 'ABS_HAT0X':
            self.dpad_left.setBrush(self.btn_on_brush if value < 0 else self.dpad_brush)
            self.dpad_right.setBrush(self.btn_on_brush if value > 0 else self.dpad_brush)
        elif code == 'ABS_HAT0Y':
            self.dpad_up.setBrush(self.btn_on_brush if value < 0 else self.dpad_brush)
            self.dpad_down.setBrush(self.btn_on_brush if value > 0 else self.dpad_brush)

    def update_trigger(self, code, value):
        # value is -1.0 to 1.0. We want 0-255.
        val_0_255 = int((value + 1) / 2 * 255)
        if code == 'ABS_Z': self.l2_label.setPlainText(f"L2: {val_0_255}")
        elif code == 'ABS_RZ': self.r2_label.setPlainText(f"R2: {val_0_255}")

# --- Main UI Window ---
class GamepadUI(QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()

    def initUI(self):
        self.setWindowTitle('RP2040 Gamepad Passthrough')
        main_layout = QVBoxLayout()
        self.setLayout(main_layout)

        self.gamepad_widget = GamepadWidget()
        main_layout.addWidget(self.gamepad_widget, alignment=Qt.AlignmentFlag.AlignCenter)

        # Device Selection and Logging UI...
        # ... (rest of the UI remains the same)
        group_label = QLabel("Device Selection")
        group_label.setFont(QFont('Arial', 12, QFont.Weight.Bold))
        main_layout.addWidget(group_label)

        gamepad_layout = QHBoxLayout()
        self.gamepad_select = QComboBox()
        self.gamepad_refresh_btn = QPushButton("Refresh")
        gamepad_layout.addWidget(QLabel("Gamepad:"))
        gamepad_layout.addWidget(self.gamepad_select, 1)
        gamepad_layout.addWidget(self.gamepad_refresh_btn)
        main_layout.addLayout(gamepad_layout)

        serial_layout = QHBoxLayout()
        self.serial_select = QComboBox()
        self.serial_refresh_btn = QPushButton("Refresh")
        self.serial_connect_btn = QPushButton("Connect")
        serial_layout.addWidget(QLabel("Serial Port:"))
        serial_layout.addWidget(self.serial_select, 1)
        serial_layout.addWidget(self.serial_refresh_btn)
        serial_layout.addWidget(self.serial_connect_btn)
        main_layout.addLayout(serial_layout)

        main_layout.addWidget(QLabel("Raw Pygame Event Monitor:"))
        self.event_monitor = QTextEdit()
        self.event_monitor.setReadOnly(True)
        self.event_monitor.setFixedHeight(100)
        main_layout.addWidget(self.event_monitor)

    def log_raw_event(self, event_string):
        self.event_monitor.append(event_string)
        self.event_monitor.verticalScrollBar().setValue(self.event_monitor.verticalScrollBar().maximum())

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ui = GamepadUI()
    ui.show()
    sys.exit(app.exec())
