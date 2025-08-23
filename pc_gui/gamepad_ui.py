import sys
from PyQt6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QGridLayout, QComboBox,
    QPushButton, QHBoxLayout, QGraphicsView, QGraphicsScene, QGraphicsEllipseItem,
    QGraphicsRectItem
)
from PyQt6.QtGui import QFont, QColor, QBrush, QPen
from PyQt6.QtCore import Qt, QObject, pyqtSignal, QRectF

# --- Signals ---
class GamepadSignals(QObject):
    stick_event = pyqtSignal(str, int)
    trigger_event = pyqtSignal(str, int)
    button_event = pyqtSignal(str, bool)
    dpad_event = pyqtSignal(str, int)
    gamepad_disconnected = pyqtSignal()

# --- Gamepad Graphics Widget ---
class GamepadWidget(QGraphicsView):
    """A widget that draws a visual representation of the gamepad."""
    def __init__(self):
        super().__init__()
        self.scene = QGraphicsScene(self)
        self.setScene(self.scene)
        self.setFixedSize(400, 200)

        # Colors
        self.bg_color = QColor("#e0e0e0")
        self.body_color = QColor("#cccccc")
        self.stick_bg_color = QColor("#bbbbbb")
        self.stick_color = QColor("#666666")
        self.btn_off_color = QColor("#aaaaaa")
        self.btn_on_color = QColor("#3399ff")

        self.scene.setBackgroundBrush(self.bg_color)
        self._draw_layout()

    def _draw_layout(self):
        # Gamepad Body
        self.scene.addRect(0, 25, 400, 150, QPen(self.body_color), QBrush(self.body_color))

        # Stick Backgrounds
        self.scene.addEllipse(50, 50, 80, 80, QPen(self.stick_bg_color), QBrush(self.stick_bg_color))

        # Movable Stick Item (origin at center of its background)
        self.left_stick = QGraphicsEllipseItem(-15, -15, 30, 30)
        self.left_stick.setBrush(QBrush(self.stick_color))
        self.left_stick.setPos(90, 90) # Start at center (50 + 80/2, 50 + 80/2)
        self.scene.addItem(self.left_stick)

        # Button Items
        self.btn_south = self._create_button_item(280, 95) # 'A'
        self.btn_east = self._create_button_item(315, 65)  # 'B'

    def _create_button_item(self, x, y):
        item = QGraphicsEllipseItem(-10, -10, 20, 20)
        item.setBrush(QBrush(self.btn_off_color))
        item.setPos(x, y)
        self.scene.addItem(item)
        return item

    # --- SLOTS for updating graphics ---
    def update_stick(self, code, value):
        # Map 0-255 value to a position offset
        offset = (value - 128) / 128.0 * 30 # Max offset of 30 pixels
        if code == 'ABS_X':
            current_y = self.left_stick.y()
            self.left_stick.setPos(90 + offset, current_y)
        elif code == 'ABS_Y':
            current_x = self.left_stick.x()
            self.left_stick.setPos(current_x, 90 + offset)

    def update_button(self, code, pressed):
        item = None
        if code == 'BTN_SOUTH':
            item = self.btn_south
        elif code == 'BTN_EAST':
            item = self.btn_east

        if item:
            item.setBrush(QBrush(self.btn_on_color) if pressed else QBrush(self.btn_off_color))

# --- Main UI Window ---
class GamepadUI(QWidget):
    """The main window containing the gamepad display and connection controls."""
    def __init__(self):
        super().__init__()
        self.initUI()

    def initUI(self):
        self.setWindowTitle('RP2040 Gamepad Passthrough')
        main_layout = QVBoxLayout()
        self.setLayout(main_layout)

        # Add the new graphics widget
        self.gamepad_widget = GamepadWidget()
        main_layout.addWidget(self.gamepad_widget, alignment=Qt.AlignmentFlag.AlignCenter)

        # --- Device Selection UI ---
        # ... (This part remains the same)
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

# Standalone testing block
if __name__ == '__main__':
    app = QApplication(sys.argv)
    ui = GamepadUI()

    # --- Example of how to test the new graphics ---
    # We now need to call the slots on the gamepad_widget
    ui.show()
    ui.gamepad_widget.update_stick('ABS_X', 255)
    ui.gamepad_widget.update_button('BTN_EAST', True)

    sys.exit(app.exec())
