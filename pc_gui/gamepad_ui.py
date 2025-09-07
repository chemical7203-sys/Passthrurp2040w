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
    imu_event = pyqtSignal(int, int, int, int, int, int) # ax, ay, az, gx, gy, gz
    gamepad_disconnected = pyqtSignal()
    device_changed = pyqtSignal()
    gamepad_list_updated = pyqtSignal(list)
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
        body_brush = QBrush(QColor("#cccccc"))
        stick_bg_brush = QBrush(QColor("#bbbbbb"))
        self.stick_brush = QBrush(QColor("#666666"))
        self.btn_off_brush = QBrush(QColor("#aaaaaa"))
        self.btn_on_brush = QBrush(QColor("#3399ff"))

        self.scene.addRect(0, 25, 450, 150, QPen(Qt.GlobalColor.transparent), body_brush)

        # Sticks
        self.scene.addEllipse(120, 90, 80, 80, QPen(Qt.GlobalColor.transparent), stick_bg_brush)
        self.left_stick = self._create_stick_item(160, 130)
        self.l3_btn = self._create_button_item(160, 130, 40)
        self.l3_btn.setZValue(-1)

        self.scene.addEllipse(250, 90, 80, 80, QPen(Qt.GlobalColor.transparent), stick_bg_brush)
        self.right_stick = self._create_stick_item(290, 130)
        self.r3_btn = self._create_button_item(290, 130, 40)
        self.r3_btn.setZValue(-1)

        # Buttons and D-Pad
        self.buttons = {
            'BTN_WEST': self._create_button_item(350, 95), 'BTN_NORTH': self._create_button_item(380, 65),
            'BTN_EAST': self._create_button_item(410, 95), 'BTN_SOUTH': self._create_button_item(380, 125),
            'DPAD_UP': self._create_dpad_item(55, 60, 20, 25), 'DPAD_DOWN': self._create_dpad_item(55, 115, 20, 25),
            'DPAD_LEFT': self._create_dpad_item(30, 85, 25, 20), 'DPAD_RIGHT': self._create_dpad_item(75, 85, 25, 20),
            'BTN_TL': self._create_dpad_item(30, 25, 80, 20), 'BTN_TR': self._create_dpad_item(340, 25, 80, 20),
            'BTN_THUMBL': self.l3_btn, 'BTN_THUMBR': self.r3_btn,
            'BTN_START': self._create_button_item(225, 45), 'BTN_SELECT': self._create_button_item(175, 45)
        }

        self.l2_label = self.scene.addText("L2: 0", QFont("Arial", 10)); self.l2_label.setPos(30, 0)
        self.r2_label = self.scene.addText("R2: 0", QFont("Arial", 10)); self.r2_label.setPos(340, 0)

    def _create_stick_item(self, x, y):
        stick = QGraphicsEllipseItem(-15, -15, 30, 30); stick.setBrush(self.stick_brush); stick.setPos(x, y); self.scene.addItem(stick)
        return stick
    def _create_button_item(self, x, y, size=20):
        button = QGraphicsEllipseItem(-size/2, -size/2, size, size); button.setBrush(self.btn_off_brush); button.setPos(x, y); self.scene.addItem(button)
        return button
    def _create_dpad_item(self, x, y, w, h):
        dpad_item = QGraphicsRectItem(0, 0, w, h); dpad_item.setBrush(self.btn_off_brush); dpad_item.setPos(x, y); self.scene.addItem(dpad_item)
        return dpad_item

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

    def update_trigger(self, code, value):
        val_0_255 = int((value + 1) / 2 * 255)
        if code == 'ABS_Z': self.l2_label.setPlainText(f"L2: {val_0_255}")
        elif code == 'ABS_RZ': self.r2_label.setPlainText(f"R2: {val_0_255}")

class GamepadUI(QWidget):
    def __init__(self):
        super().__init__(); self.initUI()
    def initUI(self):
        self.setWindowTitle('RP2040 Gamepad Passthrough'); main_layout = QVBoxLayout(); self.setLayout(main_layout)
        self.gamepad_widget = GamepadWidget(); main_layout.addWidget(self.gamepad_widget, alignment=Qt.AlignmentFlag.AlignCenter)
        group_label = QLabel("Device Selection"); group_label.setFont(QFont('Arial', 12, QFont.Weight.Bold)); main_layout.addWidget(group_label)
        gamepad_layout = QHBoxLayout(); self.gamepad_select = QComboBox(); self.gamepad_refresh_btn = QPushButton("Refresh"); gamepad_layout.addWidget(QLabel("Gamepad:")); gamepad_layout.addWidget(self.gamepad_select, 1); gamepad_layout.addWidget(self.gamepad_refresh_btn); main_layout.addLayout(gamepad_layout)
        serial_layout = QHBoxLayout(); self.serial_select = QComboBox(); self.serial_refresh_btn = QPushButton("Refresh"); self.serial_connect_btn = QPushButton("Connect"); serial_layout.addWidget(QLabel("Serial Port:")); serial_layout.addWidget(self.serial_select, 1); serial_layout.addWidget(self.serial_refresh_btn); serial_layout.addWidget(self.serial_connect_btn); main_layout.addLayout(serial_layout)
        main_layout.addWidget(QLabel("Raw Pygame Event Monitor:")); self.event_monitor = QTextEdit(); self.event_monitor.setReadOnly(True); self.event_monitor.setFixedHeight(100); main_layout.addWidget(self.event_monitor)
        main_layout.addWidget(QLabel("UART RX Monitor (from RP2040):")); self.uart_rx_monitor = QTextEdit(); self.uart_rx_monitor.setReadOnly(True); self.uart_rx_monitor.setFixedHeight(100); main_layout.addWidget(self.uart_rx_monitor)
    def log_raw_event(self, event_string):
        self.event_monitor.append(event_string); self.event_monitor.verticalScrollBar().setValue(self.event_monitor.verticalScrollBar().maximum())
    def log_uart_rx(self, data_string):
        self.uart_rx_monitor.append(data_string); self.uart_rx_monitor.verticalScrollBar().setValue(self.uart_rx_monitor.verticalScrollBar().maximum())

if __name__ == '__main__':
    app = QApplication(sys.argv); ui = GamepadUI(); ui.show(); sys.exit(app.exec())
