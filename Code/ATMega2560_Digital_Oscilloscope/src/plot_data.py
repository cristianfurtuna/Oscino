import sys
import numpy as np
import matplotlib
matplotlib.use('Qt5Agg')

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QPushButton, QVBoxLayout,
    QLabel, QLineEdit, QMessageBox, QHBoxLayout, QComboBox, QDoubleSpinBox,
    QColorDialog
)


class MplCanvas(FigureCanvas):
    def __init__(self):
        self.fig = Figure(figsize=(5, 4), dpi=100, facecolor='#fafafa')
        self.axes = self.fig.add_subplot(111)
        self.axes.set_facecolor('#000000')
        self.axes.grid(True, which='major', linestyle='-', linewidth=0.75, color='gray')
        self.axes.minorticks_on()
        self.axes.grid(True, which='minor', linestyle=':', linewidth=0.45, color='lightgray')
        self.axes.set_title("Waveform", fontsize=14)
        self.axes.set_xlabel("Time (s)")
        self.axes.set_ylabel("Voltage (V)")
        super().__init__(self.fig)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.time = None
        self.voltage = None
        self.waveform_color = 'yellow'

        self._setup_ui()

    def _setup_ui(self):
        # Channel selection
        self.channel_label = QLabel("Channel:")
        self.channel_select = QComboBox()
        self.channel_select.addItems(["1", "2", "3", "4"])

        # Timebase
        self.timebase_label = QLabel("Timebase (s/div):")
        self.timebase_input = QDoubleSpinBox()
        self.timebase_input.setRange(1e-9, 10)
        self.timebase_input.setDecimals(9)
        self.timebase_input.setValue(200e-6)

        # Voltage scale
        self.voltage_label = QLabel("Voltage Scale (V/div):")
        self.voltage_input = QDoubleSpinBox()
        self.voltage_input.setRange(1e-3, 100)
        self.voltage_input.setDecimals(3)
        self.voltage_input.setValue(1.0)

        # Buttons
        self.get_data_button = QPushButton("Generate Waveform")
        self.get_data_button.clicked.connect(self.generate_fake_waveform)

        self.color_button = QPushButton("Select Color")
        self.color_button.clicked.connect(self.select_waveform_color)

        # Layout for settings
        settings_layout = QVBoxLayout()
        settings_layout.addWidget(self.channel_label)
        settings_layout.addWidget(self.channel_select)
        settings_layout.addWidget(self.timebase_label)
        settings_layout.addWidget(self.timebase_input)
        settings_layout.addWidget(self.voltage_label)
        settings_layout.addWidget(self.voltage_input)
        settings_layout.addStretch(1)

        # Buttons layout
        buttons_layout = QHBoxLayout()
        buttons_layout.addWidget(self.get_data_button)
        buttons_layout.addWidget(self.color_button)

        # Canvas
        self.canvas = MplCanvas()

        # Combine layouts
        main_layout = QHBoxLayout()
        main_layout.addLayout(settings_layout)
        main_layout.addWidget(self.canvas)

        container_layout = QVBoxLayout()
        container_layout.addLayout(main_layout)
        container_layout.addLayout(buttons_layout)

        container = QWidget()
        container.setLayout(container_layout)
        self.setCentralWidget(container)

        self.setMinimumSize(900, 500)
        self.setWindowTitle("Nibiru Oscilloscope")

    def generate_fake_waveform(self):
        #Generate a waveform for GUI test
        t = np.linspace(0, 1, 1000)
        freq = 5
        self.time = t
        self.voltage = np.sin(2 * np.pi * freq * t)

        self.plot_waveform()

    def plot_waveform(self):
        self.canvas.axes.clear()
        self.canvas.axes.plot(self.time, self.voltage, color=self.waveform_color, linewidth=1.0)
        self.canvas.axes.set_title("Waveform")
        self.canvas.axes.set_xlabel("Time (s)")
        self.canvas.axes.set_ylabel("Voltage (V)")
        self.canvas.axes.grid(True)
        self.canvas.draw()

    def select_waveform_color(self):
        color = QColorDialog.getColor()
        if color.isValid():
            self.waveform_color = color.name()
            if self.time is not None:
                self.plot_waveform()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
