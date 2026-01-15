import sys
import serial
import numpy as np

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QComboBox, QDoubleSpinBox
)
from PyQt5.QtCore import QThread, pyqtSignal

import pyqtgraph as pg

# Constants
SYNC = b"\xAA\x55"
BYTES_PER_BATCH = 80
ADC_REF_VOLTAGE = 5.0

ADC_CLIP_HIGH = 1018
ADC_CLIP_LOW = 5


# ADC reader
class SerialADCReader:
    def __init__(self, port="COM5", baudrate=2000000):
        self.ser = serial.Serial(port, baudrate, timeout=0)
        self.rx_buf = bytearray()
        self.bit_buffer = 0
        self.bits_in_buffer = 0

    def connect(self):
        if not self.ser.is_open:
            self.ser.open()

    def disconnect(self):
        if self.ser.is_open:
            self.ser.close()

    def unpack_bits(self, data):
        samples = []
        for byte in data:
            self.bit_buffer |= (byte << self.bits_in_buffer)
            self.bits_in_buffer += 8
            while self.bits_in_buffer >= 10:
                samples.append(self.bit_buffer & 0x3FF)
                self.bit_buffer >>= 10
                self.bits_in_buffer -= 10
        return samples

    def read_frame(self):
        self.rx_buf.extend(self.ser.read(1024))
        while len(self.rx_buf) >= 2 + BYTES_PER_BATCH:
            idx = self.rx_buf.find(SYNC)
            if idx == -1:
                self.rx_buf = self.rx_buf[-200:]
                return None
            if idx + 2 + BYTES_PER_BATCH > len(self.rx_buf):
                return None
            frame = self.rx_buf[idx + 2: idx + 2 + BYTES_PER_BATCH]
            del self.rx_buf[:idx + 2 + BYTES_PER_BATCH]
            return self.unpack_bits(frame)
        return None


# Serial thread
class SerialThread(QThread):
    samples_ready = pyqtSignal(object)

    def __init__(self, port):
        super().__init__()
        self.reader = SerialADCReader(port)
        self.running = True

    def run(self):
        self.reader.connect()
        while self.running:
            samples = self.reader.read_frame()
            if samples:
                self.samples_ready.emit(samples)

    def stop(self):
        self.running = False
        self.reader.disconnect()


# Main Window (Oscilloscope)
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.serial_thread = None

        # Oscilloscope parameters
        self.timebase = 0.0005
        self.volts_per_div = 1.0
        self.horiz_divs = 10
        self.vert_divs = 8

        # Trigger / capture
        self.capture_buffer = []
        self.MAX_CAPTURE = 2000
        self.N_DISPLAY = 200

        self.TRIGGER_LEVEL_V = 2.5
        self.TRIGGER_ADC = int(
            (self.TRIGGER_LEVEL_V / ADC_REF_VOLTAGE) * 1023
        )

        self._setup_ui()
        self._setup_plot()


    def _setup_ui(self):
        self.port_input = QLineEdit("COM5")

        self.channel_select = QComboBox()
        self.channel_select.addItems(["1"])

        self.timebase_input = QDoubleSpinBox()
        self.timebase_input.setRange(1e-6, 0.01)
        self.timebase_input.setDecimals(6)
        self.timebase_input.setValue(self.timebase)
        self.timebase_input.valueChanged.connect(self.on_timebase_changed)

        self.voltage_input = QDoubleSpinBox()
        self.voltage_input.setRange(0.1, 10.0)
        self.voltage_input.setDecimals(3)
        self.voltage_input.setValue(self.volts_per_div)
        self.voltage_input.valueChanged.connect(self.on_volts_changed)

        self.connect_button = QPushButton("Connect")
        self.connect_button.clicked.connect(self.connect_adc)

        self.disconnect_button = QPushButton("Disconnect")
        self.disconnect_button.clicked.connect(self.disconnect_adc)
        self.disconnect_button.setEnabled(False)

        left = QVBoxLayout()
        left.addWidget(QLabel("Serial Port:"))
        left.addWidget(self.port_input)
        left.addWidget(QLabel("Channel:"))
        left.addWidget(self.channel_select)
        left.addWidget(QLabel("Timebase (s/div):"))
        left.addWidget(self.timebase_input)
        left.addWidget(QLabel("Voltage (V/div):"))
        left.addWidget(self.voltage_input)
        left.addWidget(self.connect_button)
        left.addWidget(self.disconnect_button)
        left.addStretch(1)

        self.plot_widget = pg.GraphicsLayoutWidget()
        self.plot = self.plot_widget.addPlot(title="Nibiru Oscilloscope")
        self.curve = self.plot.plot(pen=pg.mkPen("y", width=2))

        layout = QHBoxLayout()
        layout.addLayout(left)
        layout.addWidget(self.plot_widget)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        self.setWindowTitle("Nibiru Oscilloscope")
        self.resize(1100, 550)


    def _setup_plot(self):
        pg.setConfigOptions(antialias=True, background="k", foreground="w")
        self.plot.showGrid(x=True, y=True, alpha=0.4)
        self.update_axes()

    def update_axes(self):
        tspan = self.timebase * self.horiz_divs
        vspan = self.volts_per_div * self.vert_divs

        self.plot.setXRange(-tspan / 2, tspan / 2)
        self.plot.setYRange(-vspan / 2, vspan / 2)

        self.plot.setLabel("bottom", f"Time  {self.timebase:.6f} s/div")
        self.plot.setLabel("left", f"Voltage  {self.volts_per_div:.3f} V/div")

    def on_timebase_changed(self, val):
        self.timebase = val
        self.update_axes()

    def on_volts_changed(self, val):
        self.volts_per_div = val
        self.update_axes()


    def connect_adc(self):
        port = self.port_input.text()
        self.serial_thread = SerialThread(port)
        self.serial_thread.samples_ready.connect(self.on_samples)
        self.serial_thread.start()

        self.connect_button.setEnabled(False)
        self.disconnect_button.setEnabled(True)

    def disconnect_adc(self):
        if self.serial_thread:
            self.serial_thread.stop()
            self.serial_thread.wait()
            self.serial_thread = None

        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)

    # Smart trigger + display
    def on_samples(self, samples):
        self.capture_buffer.extend(samples)
        if len(self.capture_buffer) < self.MAX_CAPTURE:
            return

        buf = np.array(
            self.capture_buffer[:self.MAX_CAPTURE],
            dtype=np.int16
        )

        # Detect DC signal
        is_dc = np.ptp(buf) < 4

        trigger_index = None
        if not is_dc:
            for i in range(1, len(buf)):
                if (
                    buf[i] >= self.TRIGGER_ADC
                    and buf[i - 1] < self.TRIGGER_ADC
                    and ADC_CLIP_LOW < buf[i] < ADC_CLIP_HIGH
                    and (buf[i] - buf[i - 1]) > 4
                ):
                    trigger_index = i
                    break

        if trigger_index is None:
            window = buf[-self.N_DISPLAY:]
        else:
            start = trigger_index
            end = start + self.N_DISPLAY
            window = buf[start:end] if end <= len(buf) else buf[-self.N_DISPLAY:]

        volts = (window.astype(np.float32) / 1023.0) * ADC_REF_VOLTAGE

        # Clamp
        volts = np.minimum(
            volts,
            ADC_REF_VOLTAGE - ADC_REF_VOLTAGE / 1024
        )

        # Filter for AC signals
        if not is_dc and len(volts) >= 12:
            volts = np.convolve(volts, np.ones(12) / 12.0, mode="same")

        tspan = self.timebase * self.horiz_divs
        t = np.linspace(-tspan / 2, tspan / 2, len(volts))

        self.curve.setData(t, volts)

        self.capture_buffer.clear()

# Main
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
