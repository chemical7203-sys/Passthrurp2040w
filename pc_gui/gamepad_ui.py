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
    # These are no longer used by the handler but can be kept for other purposes
    stick_event = pyqtSignal(str, float)
    trigger_event = pyqtSignal(str, float)
    button_event = pyqtSignal(str, bool)
    gamepad_disconnected = pyqtSignal()
    # These are still useful
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
        self.scene.addEllipse(250, 90, 80, 80, QPen(Qt.GlobalColor.transparent), stick_bg_brush)
        self.right_stick = self._create_stick_item(290, 130)

        # Buttons - we don't need a map anymore as UI is updated from state
        self.l2_label = self.scene.addText("L2: 0", QFont("Arial", 10)); self.l2_label.setPos(30, 0)
        self.r2_label = self.scene.addText("R2: 0", QFont("Arial", 10)); self.r2_label.setPos(340, 0)

    def _create_stick_item(self, x, y):
        stick = QGraphicsEllipseItem(-15, -15, 30, 30); stick.setBrush(self.stick_brush); stick.setPos(x, y); self.scene.addItem(stick)
        return stick

    def update_stick(self, code, value):
        if code == 'ABS_X': self.stick_x_val = value
        elif code == 'ABS_Y': self.stick_y_val = value
        elif code == 'ABS_RX': self.rstick_x_val = value
        elif code == 'ABS_RY': self.rstick_y_val = value
        self.left_stick.setPos(160 + (self.stick_x_val * 30), 130 + (self.stick_y_val * 30))
        self.right_stick.setPos(290 + (self.rstick_x_val * 30), 130 + (self.rstick_y_val * 30))

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

        # --- Device Status Section ---
        group_label = QLabel("Device Status"); group_label.setFont(QFont('Arial', 12, QFont.Weight.Bold)); main_layout.addWidget(group_label)

        status_layout = QHBoxLayout()
        status_layout.addWidget(QLabel("Gamepad:"))
        self.gamepad_status_label = QLabel("Disconnected"); self.gamepad_status_label.setFont(QFont('Arial', 10, QFont.Weight.Bold))
        status_layout.addWidget(self.gamepad_status_label, 1)
        main_layout.addLayout(status_layout)

        serial_layout = QHBoxLayout(); self.serial_select = QComboBox(); self.serial_refresh_btn = QPushButton("Refresh"); self.serial_connect_btn = QPushButton("Connect"); serial_layout.addWidget(QLabel("Serial Port:")); serial_layout.addWidget(self.serial_select, 1); serial_layout.addWidget(self.serial_refresh_btn); serial_layout.addWidget(self.serial_connect_btn); main_layout.addLayout(serial_layout)

        # --- Monitors ---
        main_layout.addWidget(QLabel("Controller Data Monitor:")); self.event_monitor = QTextEdit(); self.event_monitor.setReadOnly(True); self.event_monitor.setFixedHeight(100); main_layout.addWidget(self.event_monitor)
        main_layout.addWidget(QLabel("UART RX Monitor (from RP2040):")); self.uart_rx_monitor = QTextEdit(); self.uart_rx_monitor.setReadOnly(True); self.uart_rx_monitor.setFixedHeight(100); main_layout.addWidget(self.uart_rx_monitor)

    def set_gamepad_status(self, status_text):
        self.gamepad_status_label.setText(status_text)

    def log_raw_event(self, event_string):
        self.event_monitor.append(event_string); self.event_monitor.verticalScrollBar().setValue(self.event_monitor.verticalScrollBar().maximum())

    def log_uart_rx(self, data_string):
        self.uart_rx_monitor.append(data_string); self.uart_rx_monitor.verticalScrollBar().setValue(self.uart_rx_monitor.verticalScrollBar().maximum())

if __name__ == '__main__':
    app = QApplication(sys.argv); ui = GamepadUI(); ui.show(); sys.exit(app.exec())
