#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import time

import hal

try:
    from PySide6 import QtCore, QtGui, QtWidgets
    QT_BINDING = "PySide6"
except ImportError:
    try:
        from PyQt5 import QtCore, QtGui, QtWidgets
        QT_BINDING = "PyQt5"
    except ImportError as exc:
        print(
            "Qt bindings are required. Install with: pip install PySide6 (or PyQt5)",
            file=sys.stderr,
        )
        raise SystemExit(1) from exc


class ScopeWidget(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(140)
        self.samples = [0.0] * 320
        self.current_value = 0.0
        self.encoder_idx = 0

    def set_encoder(self, idx):
        self.encoder_idx = idx
        self.samples = [0.0] * len(self.samples)
        self.update()

    def push_value(self, value):
        self.current_value = float(value)
        self.samples.pop(0)
        self.samples.append(self.current_value)
        self.update()

    def paintEvent(self, event):
        del event
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)

        rect = self.rect().adjusted(2, 2, -2, -2)
        painter.fillRect(rect, QtGui.QColor(15, 18, 24))
        painter.setPen(QtGui.QPen(QtGui.QColor(95, 110, 130), 2))
        painter.drawRoundedRect(rect, 8, 8)

        center_y = rect.center().y()
        painter.setPen(QtGui.QPen(QtGui.QColor(75, 85, 100), 1))
        painter.drawLine(rect.left() + 8, center_y, rect.right() - 8, center_y)

        max_abs = max(0.2, max(abs(v) for v in self.samples))
        amp = max(8, (rect.height() // 2) - 12)

        if len(self.samples) > 1:
            points = []
            width = max(1, rect.width() - 20)
            for i, val in enumerate(self.samples):
                x = rect.left() + 10 + int(i * width / (len(self.samples) - 1))
                y = center_y - int((val / max_abs) * amp)
                points.append(QtCore.QPoint(x, y))
            painter.setPen(QtGui.QPen(QtGui.QColor(80, 235, 160), 2))
            painter.drawPolyline(QtGui.QPolygon(points))

        painter.setPen(QtGui.QColor(210, 220, 235))
        text = (
            f"ENC{self.encoder_idx} velocity scope (RPS)  now={self.current_value:+.4f}  "
            f"scale=+/-{max_abs:.3f}"
        )
        painter.drawText(rect.adjusted(12, 8, -12, -8), QtCore.Qt.AlignTop | QtCore.Qt.AlignLeft, text)


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def load_postgui_hal(halfile):
    cmd = ["halcmd", "-f", halfile]
    result = subprocess.run(cmd, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        print("Failed to load postgui HAL file:", halfile, file=sys.stderr)
        if result.stdout:
            print(result.stdout, file=sys.stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        raise SystemExit(result.returncode)


def start_halrun_nonblocking(script_path):
    return None


def create_component():
    comp = hal.component("bb1-ui")

    comp.newpin("connected", hal.HAL_BIT, hal.HAL_IN)
    comp.newpin("io_ready", hal.HAL_BIT, hal.HAL_IN)
    comp.newpin("jitter", hal.HAL_S32, hal.HAL_IN)

    for i in range(16):
        comp.newpin(f"in{i:02d}", hal.HAL_BIT, hal.HAL_IN)

    for i in range(8):
        comp.newpin(f"out{i:02d}", hal.HAL_BIT, hal.HAL_OUT)

    comp.newpin("analog0", hal.HAL_FLOAT, hal.HAL_OUT)
    comp.newpin("analog1", hal.HAL_FLOAT, hal.HAL_OUT)

    comp.newpin("enc0_raw_count", hal.HAL_S32, hal.HAL_IN)
    comp.newpin("enc0_position", hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin("enc0_velocity_rps", hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin("enc0_velocity_rpm", hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin("enc0_index_enable", hal.HAL_BIT, hal.HAL_IO)
    comp.newpin("enc1_velocity_rps", hal.HAL_FLOAT, hal.HAL_IN)

    comp.newpin("heartbeat", hal.HAL_U32, hal.HAL_OUT)

    for i in range(8):
        comp[f"out{i:02d}"] = 0
    comp["analog0"] = 0.0
    comp["analog1"] = 0.0
    comp["enc0_index_enable"] = 0
    comp["heartbeat"] = 0

    comp.ready()
    return comp


def create_component_with_retry(max_attempts=40, delay_s=0.2):
    last_exc = None
    for _ in range(max_attempts):
        try:
            return create_component()
        except Exception as exc:
            last_exc = exc
            time.sleep(delay_s)
    print(
        f"Failed to create HAL component after {max_attempts} attempts: {last_exc}",
        file=sys.stderr,
    )
    raise SystemExit(1)


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, comp):
        super().__init__()
        self.comp = comp
        self.out_state = [False] * 8
        self.scope_encoder = 0
        self.last_hb = time.monotonic()

        self.setWindowTitle(f"Stepper-Ninja BreakoutBoard1 Test Panel (Qt: {QT_BINDING})")
        self.resize(1080, 760)

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        root = QtWidgets.QVBoxLayout(central)

        title = QtWidgets.QLabel("BreakoutBoard 1 Test Panel")
        title.setStyleSheet("font-size: 22px; font-weight: 600; color: rgb(240,240,240);")
        root.addWidget(title)

        self.enc_big = QtWidgets.QLabel("ENC0 POS 0.0000")
        self.enc_big.setStyleSheet("font-size: 36px; font-weight: 700; color: rgb(255,220,120);")
        root.addWidget(self.enc_big)

        self.status = QtWidgets.QLabel("")
        self.enc_status = QtWidgets.QLabel("")
        hint = QtWidgets.QLabel(
            "Keys: 1..8 outputs, Q/A analog0, W/S analog1, I index-enable, E scope ENC0/1, Esc quit"
        )
        hint.setStyleSheet("color: rgb(180,190,205);")
        self.status.setStyleSheet("color: rgb(200,210,220);")
        self.enc_status.setStyleSheet("color: rgb(200,210,220);")
        root.addWidget(self.status)
        root.addWidget(self.enc_status)
        root.addWidget(hint)

        inputs_group = QtWidgets.QGroupBox("Inputs")
        inputs_layout = QtWidgets.QGridLayout(inputs_group)
        self.in_leds = []
        for i in range(16):
            led = QtWidgets.QLabel(f"IN{i:02d}")
            led.setAlignment(QtCore.Qt.AlignCenter)
            led.setMinimumSize(92, 42)
            led.setStyleSheet(
                "border: 1px solid rgb(90,90,90); border-radius: 8px; "
                "background: rgb(70,70,70); color: rgb(220,220,220);"
            )
            self.in_leds.append(led)
            inputs_layout.addWidget(led, i // 8, i % 8)
        root.addWidget(inputs_group)

        output_group = QtWidgets.QGroupBox("Outputs")
        output_layout = QtWidgets.QGridLayout(output_group)
        self.out_buttons = []
        for i in range(8):
            btn = QtWidgets.QPushButton(f"OUT{i:02d}")
            btn.setCheckable(True)
            btn.setMinimumSize(100, 48)
            btn.toggled.connect(self._on_output_toggled)
            self.out_buttons.append(btn)
            output_layout.addWidget(btn, 0, i)
        root.addWidget(output_group)

        analog_group = QtWidgets.QGroupBox("Analog Outputs")
        analog_layout = QtWidgets.QGridLayout(analog_group)
        self.analog0_slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.analog0_slider.setRange(0, 100)
        self.analog0_slider.setSingleStep(1)
        self.analog0_slider.setPageStep(5)
        self.analog0_value = QtWidgets.QLabel("0.0 V")
        self.analog0_value.setMinimumWidth(58)

        self.analog1_slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.analog1_slider.setRange(0, 100)
        self.analog1_slider.setSingleStep(1)
        self.analog1_slider.setPageStep(5)
        self.analog1_value = QtWidgets.QLabel("0.0 V")
        self.analog1_value.setMinimumWidth(58)

        analog_layout.addWidget(QtWidgets.QLabel("analog0"), 0, 0)
        analog_layout.addWidget(self.analog0_slider, 0, 1)
        analog_layout.addWidget(self.analog0_value, 0, 2)
        analog_layout.addWidget(QtWidgets.QLabel("analog1"), 1, 0)
        analog_layout.addWidget(self.analog1_slider, 1, 1)
        analog_layout.addWidget(self.analog1_value, 1, 2)

        self.analog0_slider.valueChanged.connect(
            lambda value: self.analog0_value.setText(f"{value / 10.0:.1f} V")
        )
        self.analog1_slider.valueChanged.connect(
            lambda value: self.analog1_value.setText(f"{value / 10.0:.1f} V")
        )
        root.addWidget(analog_group)

        scope_row = QtWidgets.QHBoxLayout()
        self.scope_switch = QtWidgets.QPushButton("SCOPE ENC0")
        self.scope_switch.clicked.connect(self.toggle_scope_encoder)
        self.index_btn = QtWidgets.QPushButton("Request ENC0 Index")
        self.index_btn.clicked.connect(self.request_index_enable)
        scope_row.addWidget(self.scope_switch)
        scope_row.addWidget(self.index_btn)
        scope_row.addStretch(1)
        root.addLayout(scope_row)

        self.scope = ScopeWidget()
        root.addWidget(self.scope)

        central.setStyleSheet(
            "QWidget { background-color: rgb(22,25,30); color: rgb(235,235,235); }"
            "QGroupBox { border: 1px solid rgb(85,95,110); margin-top: 10px; padding-top: 10px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
            "QPushButton { background: rgb(55,55,55); border: 1px solid rgb(130,130,130); border-radius: 7px; padding: 6px; }"
            "QPushButton:checked { background: rgb(30,150,230); }"
        )

        shortcut_cls = getattr(QtGui, "QShortcut", None) or getattr(QtWidgets, "QShortcut")
        self._shortcuts = []
        for i in range(8):
            self._shortcuts.append(
                shortcut_cls(QtGui.QKeySequence(str(i + 1)), self, activated=lambda x=i: self.toggle_output(x))
            )
        self._shortcuts.append(
            shortcut_cls(QtGui.QKeySequence("Q"), self, activated=lambda: self.bump_analog(self.analog0_slider, +1))
        )
        self._shortcuts.append(
            shortcut_cls(QtGui.QKeySequence("A"), self, activated=lambda: self.bump_analog(self.analog0_slider, -1))
        )
        self._shortcuts.append(
            shortcut_cls(QtGui.QKeySequence("W"), self, activated=lambda: self.bump_analog(self.analog1_slider, +1))
        )
        self._shortcuts.append(
            shortcut_cls(QtGui.QKeySequence("S"), self, activated=lambda: self.bump_analog(self.analog1_slider, -1))
        )
        self._shortcuts.append(shortcut_cls(QtGui.QKeySequence("I"), self, activated=self.request_index_enable))
        self._shortcuts.append(shortcut_cls(QtGui.QKeySequence("E"), self, activated=self.toggle_scope_encoder))
        self._shortcuts.append(shortcut_cls(QtGui.QKeySequence("Escape"), self, activated=self.close))

        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.on_tick)
        self.timer.start(33)

    def _on_output_toggled(self, checked):
        del checked
        for i, btn in enumerate(self.out_buttons):
            self.out_state[i] = bool(btn.isChecked())

    def toggle_output(self, idx):
        self.out_buttons[idx].toggle()

    def bump_analog(self, slider, delta_steps):
        slider.setValue(int(clamp(slider.value() + delta_steps, slider.minimum(), slider.maximum())))

    def analog_value(self, slider):
        return float(slider.value()) / 10.0

    def request_index_enable(self):
        self.comp["enc0_index_enable"] = 1

    def toggle_scope_encoder(self):
        self.scope_encoder = 1 - self.scope_encoder
        self.scope_switch.setText(f"SCOPE ENC{self.scope_encoder}")
        self.scope.set_encoder(self.scope_encoder)

    def on_tick(self):
        for i in range(8):
            self.comp[f"out{i:02d}"] = int(self.out_state[i])
        analog0 = self.analog_value(self.analog0_slider)
        analog1 = self.analog_value(self.analog1_slider)
        self.comp["analog0"] = analog0
        self.comp["analog1"] = analog1

        now = time.monotonic()
        if now - self.last_hb > 0.1:
            self.comp["heartbeat"] = (int(self.comp["heartbeat"]) + 1) & 0xFFFFFFFF
            self.last_hb = now

        for i in range(16):
            on = bool(self.comp[f"in{i:02d}"])
            bg = "rgb(40,200,80)" if on else "rgb(80,80,80)"
            self.in_leds[i].setStyleSheet(
                "border: 1px solid rgb(90,90,90); border-radius: 8px; "
                f"background: {bg}; color: rgb(220,220,220);"
            )

        self.status.setText(
            f"connected={int(self.comp['connected'])}  io_ready={int(self.comp['io_ready'])}  "
            f"analog0={analog0:.2f}  analog1={analog1:.2f} "
            f"jitter={int(self.comp['jitter'])}us"
        )
        self.enc_status.setText(
            f"enc0_raw={int(self.comp['enc0_raw_count'])}  "
            f"enc0_pos={float(self.comp['enc0_position']):.4f}  "
            f"enc0_rps={float(self.comp['enc0_velocity_rps']):.4f}  "
            f"enc0_rpm={float(self.comp['enc0_velocity_rpm']):.2f}  "
            f"index_en={int(self.comp['enc0_index_enable'])}"
        )
        self.enc_big.setText(f"ENC0 POS {float(self.comp['enc0_position']):.4f}")

        enc0_rps = float(self.comp["enc0_velocity_rps"])
        enc1_rps = float(self.comp["enc1_velocity_rps"])
        scope_val = enc0_rps if self.scope_encoder == 0 else enc1_rps
        self.scope.push_value(scope_val)


def main():
    parser = argparse.ArgumentParser(description="BreakoutBoard 1 HAL Qt test panel")
    parser.add_argument(
        "--postgui",
        default=os.path.join(os.path.dirname(__file__), "bb1_postgui.hal"),
        help="Path to postgui HAL file (default: %(default)s)",
    )
    parser.add_argument(
        "--skip-load-postgui",
        action="store_true",
        help="Do not load postgui HAL file (useful if already loaded)",
    )
    parser.add_argument(
        "--halrun-script",
        default=os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "halrun-stepper-ninja.sh")),
        help="Path to halrun script started before creating HAL component (default: %(default)s)",
    )
    parser.add_argument(
        "--skip-halrun",
        action="store_true",
        help="Do not auto-start halrun script",
    )
    args = parser.parse_args()

    if not args.skip_halrun:
        start_halrun_nonblocking(args.halrun_script)

    comp = create_component_with_retry()

    if not args.skip_load_postgui:
        load_postgui_hal(args.postgui)

    app = QtWidgets.QApplication(sys.argv)
    win = MainWindow(comp)
    win.show()
    if hasattr(app, "exec"):
        return app.exec()
    return app.exec_()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        pass
