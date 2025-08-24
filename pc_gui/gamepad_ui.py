import sys
from PyQt6.QtWidgets import (
    QApplication, QWidget, QLabel, QVBoxLayout, QGridLayout, QComboBox,
    QPushButton, QHBoxLayout, QGraphicsView, QGraphicsScene, QGraphicsEllipseItem,
    QGraphicsRectItem, QTextEdit
)
from PyQt6.QtGui import QFont, QColor, QBrush, QPen
from PyQt6.QtCore import Qt, QObject, pyqtSignal, QRectF

# --- Signals ---
class GamepadSignals(QObject):
    stick_event = pyqtSignal(str, float)
    trigger_event = pyqtSignal(str, float)
    button_event = pyqtSignal(str, bool)
    dpad_event = pyqtSignal(str, int)
    gamepad_disconnected = pyqtSignal()
    raw_event = pyqtSignal(str) # For the raw event monitor

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

        # State for stick position
        self.stick_x_val = 0.0
        self.stick_y_val = 0.0

        self._draw_layout()

    def _draw_layout(self):
        self.scene.addRect(0, 25, 400, 150, QPen(self.body_color), QBrush(self.body_color))
        self.scene.addEllipse(50, 50, 80, 80, QPen(self.stick_bg_color), QBrush(self.stick_bg_color))

        self.left_stick = QGraphicsEllipseItem(-15, -15, 30, 30)
        self.left_stick.setBrush(QBrush(self.stick_color))
        self.left_stick.setPos(90, 90)
        self.scene.addItem(self.left_stick)

        self.btn_south = self._create_button_item(280, 95)
        self.btn_east = self._create_button_item(315, 65)

    def _create_button_item(self, x, y):
        item = QGraphicsEllipseItem(-10, -10, 20, 20)
        item.setBrush(QBrush(self.btn_off_color))
        item.setPos(x, y)
        self.scene.addItem(item)
        return item

    # --- SLOTS for updating graphics ---
    def update_stick(self, code, value):
        # value is a float from -1.0 to 1.0
        if code == 'ABS_X':
            self.stick_x_val = value
        elif code == 'ABS_Y':
            self.stick_y_val = value

        # Calculate position based on state
        # Max offset of 30 pixels from center (90, 90)
        new_x = 90 + (self.stick_x_val * 30)
        new_y = 90 + (self.stick_y_val * 30)
        self.left_stick.setPos(new_x, new_y)

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

        self.gamepad_widget = GamepadWidget()
        main_layout.addWidget(self.gamepad_widget, alignment=Qt.AlignmentFlag.AlignCenter)

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

        # --- Raw Event Monitor ---
        main_layout.addWidget(QLabel("Raw Pygame Event Monitor:"))
        self.event_monitor = QTextEdit()
        self.event_monitor.setReadOnly(True)
        self.event_monitor.setFixedHeight(100)
        main_layout.addWidget(self.event_monitor)

    def log_raw_event(self, event_string):
        """Appends a string to the event monitor."""
        self.event_monitor.append(event_string)
        # Auto-scroll to the bottom
        self.event_monitor.verticalScrollBar().setValue(self.event_monitor.verticalScrollBar().maximum())

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ui = GamepadUI()
    ui.show()
    sys.exit(app.exec())
