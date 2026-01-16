import sys
import time
import serial
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QDial, QGridLayout, QFrame, QGroupBox
)
from PyQt5.QtCore import QThread, pyqtSignal, Qt
import pyqtgraph as pg

# Constants
SYNC_SEQ = b"\xAA\x55" # Sync bytes
BATCH_SIZE = 600 # Size of ADC data batch
ADC_REF_VOLTAGE = 5.0  # Reference voltage of ADC = VDD (5V)

# Step values for volts/div and timescale
TIME_STEPS = [0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.010, 0.020, 0.050, 0.100]
VOLT_STEPS = [0.1, 0.2, 0.5, 1.0, 2.0, 5.0]

class SerialADCReader:
    # Configure port, baud rate and timeout
    def __init__(self, port="COM5", baudrate=2000000):
        self.ser = serial.Serial(port, baudrate, timeout=0.01)
        self.rx_buf = bytearray() # Internal buffer for received data

    # Open the port
    def connect(self):
        if not self.ser.is_open:
            self.ser.open()
            self.ser.reset_input_buffer() # Reset internal buffer to get rid of junk data from previous runs

    # Close the port
    def disconnect(self):
        if self.ser.is_open:
            self.ser.close()

    def read_frame(self):
        # Read everything on the serial buffer
        if self.ser.in_waiting > 0:
            data = self.ser.read(self.ser.in_waiting)
            self.rx_buf.extend(data) # Add everything from the serial buffer into our internal buffer for further processing

        frames_found = []
        while True:
            # Search the buffer for Sync bits to find a batch
            idx = self.rx_buf.find(SYNC_SEQ)
            # If there are no Sync bits found and the buffer has grown in size too much
            # Clear the buffer and keep only the last 100 bytes
            if idx == -1:
                if len(self.rx_buf) > 2 * BATCH_SIZE:
                    self.rx_buf = self.rx_buf[-100:] 
                break
            
            # Verify if there are enough bytes for a full batch
            # If there are not, wait for full batch
            pkt_len = 2 + BATCH_SIZE # 2 bytes Sync + batch size
            if len(self.rx_buf) < idx + pkt_len:
                break 

            payload = self.rx_buf[idx + 2 : idx + pkt_len] # Skip sync bytes and extract only the necessary data
            raw_data = np.frombuffer(payload, dtype=np.uint8) # Convert into decimal values (0-255 for 8 bits)
            # Scale the values into (0 - 1020) to simulate 10 bit ADC
            # Added so I don't have to modify my program (previously used to send all 10 bits of ADC data)
            scaled_data = raw_data.astype(np.int16) * 4
            frames_found.extend(scaled_data)
            del self.rx_buf[:idx + pkt_len]

        return frames_found if frames_found else None

    # Preluare date din interfata seriala pe un thread separat, fara sa blochez GUI-ul
class SerialThread(QThread):
    samples_ready = pyqtSignal(object) # Anunta programul principal ca am citit un batch pentru display

    # Initializare citire buffer serial
    def __init__(self, port):
        super().__init__()
        self.port = port
        self.running = False
        self.reader = None

    def run(self):
        try:
            self.reader = SerialADCReader(self.port)
            self.reader.connect()
            self.running = True
            #In timp ce citesc datele, daca am un frame complet, il trimit catre programul principal pentru prelucrare
            while self.running:
                samples = self.reader.read_frame()
                if samples:
                    self.samples_ready.emit(samples)
                else:
                    self.msleep(1) # Mic delay daca inca nu au aparut date ca sa nu blocam thread-ul inutil
        except Exception as e:
            print(f"Serial Error: {e}")  #In caz de eroare pe serial read
        finally:
            if self.reader:
                self.reader.disconnect() # Inchidere port serial (cu sau fara eroare)

    def stop(self):
        self.running = False
        self.wait()

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.serial_thread = None
        
        # Buffer circular
        self.capture_buffer = []
        self.MAX_CAPTURE = 500000 # Buffer mare
        
        # Variabile interne
        self.measured_sps = 60000.0 # Valoare default pana la masura
        self.total_samples_received = 0
        self.last_time_check = time.time()
        
        # Scope state
        self.time_idx = 4   # Default 2ms (index 4 in TIME_STEPS)
        self.volt_idx = 3   # Default 1V (index 3 in VOLT_STEPS)
        self.trigger_lvl = 2.5
        self.vert_offset = 0.0
        
        self.last_draw_time = 0
        self.FPS_LIMIT = 30  
        self.last_valid_frame = None 
        self.trigger_lost_timer = 0

        self._setup_ui()
        self._setup_plot()
        self.update_scope_settings() # Apply initial settings

    def _setup_ui(self):
        # Styling dial buttons
        dial_style = """
            QDial { background-color: #333; }
        """
        
        # Zona de conectare
        self.port_input = QLineEdit("COM5") 
        self.port_input.setFixedWidth(80)
        self.btn_connect = QPushButton("ON")
        self.btn_connect.setCheckable(True)
        self.btn_connect.clicked.connect(self.toggle_connection)
        self.btn_connect.setStyleSheet("QPushButton { background-color: #444; color: white; border-radius: 5px; font-weight: bold; } QPushButton:checked { background-color: #00E676; color: black; }")
        self.btn_connect.setFixedSize(50, 50)
        
        conn_group = QGroupBox("Input")
        conn_layout = QVBoxLayout()
        conn_layout.addWidget(QLabel("PORT"))
        conn_layout.addWidget(self.port_input)
        conn_layout.addWidget(self.btn_connect)
        conn_layout.addStretch()
        conn_group.setLayout(conn_layout)

        # Zona Verticala (Volts/Div & Position - offset)
        self.dial_volts = QDial()
        self.dial_volts.setRange(0, len(VOLT_STEPS)-1)
        self.dial_volts.setValue(self.volt_idx)
        self.dial_volts.setNotchesVisible(True)
        self.dial_volts.valueChanged.connect(self.update_scope_settings)
        self.lbl_volts = QLabel("1 V")
        self.lbl_volts.setAlignment(Qt.AlignCenter)

        self.dial_offset = QDial()
        self.dial_offset.setRange(-60, 60) # +/- 6.0 Diviziuni
        self.dial_offset.setValue(0)
        self.dial_offset.setNotchesVisible(True)
        self.dial_offset.valueChanged.connect(self.update_scope_settings)
        self.lbl_offset = QLabel("Pos: 0 Div")
        self.lbl_offset.setAlignment(Qt.AlignCenter)

        vert_group = QGroupBox("Vertical")
        vert_layout = QVBoxLayout()
        vert_layout.addWidget(QLabel("SCALE"))
        vert_layout.addWidget(self.dial_volts)
        vert_layout.addWidget(self.lbl_volts)
        vert_layout.addWidget(QLabel("POSITION"))
        vert_layout.addWidget(self.dial_offset)
        vert_layout.addWidget(self.lbl_offset)
        vert_group.setLayout(vert_layout)

        # Zona Orizontala (Time/Div)
        self.dial_time = QDial()
        self.dial_time.setRange(0, len(TIME_STEPS)-1)
        self.dial_time.setValue(self.time_idx)
        self.dial_time.setNotchesVisible(True)
        self.dial_time.valueChanged.connect(self.update_scope_settings)
        self.lbl_time = QLabel("2 ms")
        self.lbl_time.setAlignment(Qt.AlignCenter)

        horiz_group = QGroupBox("Horizontal")
        horiz_layout = QVBoxLayout()
        horiz_layout.addWidget(QLabel("TIME/DIV"))
        horiz_layout.addWidget(self.dial_time)
        horiz_layout.addWidget(self.lbl_time)
        horiz_layout.addStretch()
        horiz_group.setLayout(horiz_layout)

        # Zona Trigger
        self.dial_trig = QDial()
        self.dial_trig.setRange(0, 50) # 0.0 la 5.0V
        self.dial_trig.setValue(25)
        self.dial_trig.setNotchesVisible(True)
        self.dial_trig.valueChanged.connect(self.update_scope_settings)
        self.lbl_trig_val = QLabel("T: 2.5V")
        self.lbl_trig_val.setAlignment(Qt.AlignCenter)
        
        self.lbl_trig_status = QLabel("NO SIG")
        self.lbl_trig_status.setStyleSheet("color: gray; font-weight: bold; border: 1px solid gray; padding: 2px;")
        self.lbl_trig_status.setAlignment(Qt.AlignCenter)

        trig_group = QGroupBox("Trigger")
        trig_layout = QVBoxLayout()
        trig_layout.addWidget(QLabel("LEVEL"))
        trig_layout.addWidget(self.dial_trig)
        trig_layout.addWidget(self.lbl_trig_val)
        trig_layout.addWidget(self.lbl_trig_status)
        trig_group.setLayout(trig_layout)

        # Panou Control (stanga)
        ctrl_layout = QVBoxLayout()
        ctrl_layout.addWidget(conn_group)
        ctrl_layout.addWidget(vert_group)
        ctrl_layout.addWidget(horiz_group)
        ctrl_layout.addWidget(trig_group)
        ctrl_layout.addStretch()
        
        ctrl_widget = QWidget()
        ctrl_widget.setLayout(ctrl_layout)
        ctrl_widget.setFixedWidth(180) # Latime fixa a panoului

        # Plot Area
        self.plot_widget = pg.GraphicsLayoutWidget()
        self.plot_widget.setBackground('#111') # Background inchis
        self.plot = self.plot_widget.addPlot(title="NIBIRUSCOPE")
        self.curve = self.plot.plot(pen=pg.mkPen('#00E676', width=2)) 
        
        # Setup Grid
        self.plot.showGrid(x=True, y=True, alpha=0.3)
        self.plot.setLabel('bottom', 'Time', units='s')
        self.plot.setLabel('left', 'Voltage', units='V')

        # Main Layout
        main_layout = QHBoxLayout()
        main_layout.addWidget(ctrl_widget)
        main_layout.addWidget(self.plot_widget)

        container = QWidget()
        container.setLayout(main_layout)
        self.setCentralWidget(container)
        self.setWindowTitle("NibiruScope")
        self.resize(1200, 700)

    def _setup_plot(self):
        pg.setConfigOptions(antialias=False) 

    def update_scope_settings(self):
        # Timebase
        idx_t = self.dial_time.value()
        time_val = TIME_STEPS[idx_t]
        
        if time_val < 0.001:
            self.lbl_time.setText(f"{time_val*1000000:.0f} µs")
        elif time_val < 1.0:
            self.lbl_time.setText(f"{time_val*1000:.0f} ms")
        else:
            self.lbl_time.setText(f"{time_val:.2f} s")
            
        # Volts/div
        idx_v = self.dial_volts.value()
        volt_per_div = VOLT_STEPS[idx_v]
        self.lbl_volts.setText(f"{volt_per_div} V")
        
        # Position - Offset
        # Modifica valoarea dial-ului in diviziuni
        offset_in_divs = self.dial_offset.value() / 10.0
        
        # Scalare pentru diferite valori de volts/div
        self.vert_offset = offset_in_divs * volt_per_div 
        
        # Afisare label offset scalat in volti pentru dial
        self.lbl_offset.setText(f"Pos: {self.vert_offset:+.1f}V")

        # Trigger
        trig_raw = self.dial_trig.value()
        self.trigger_lvl = trig_raw / 10.0
        self.lbl_trig_val.setText(f"T: {self.trigger_lvl:.1f}V")
        
        
        # Axa X
        self.plot.setXRange(0, time_val * 10, padding=0)
        
        # Axa Y:
        half_screen_volts = volt_per_div * 4 
        self.plot.setYRange(-half_screen_volts, half_screen_volts, padding=0)

        # Functionalitate on/off
    def toggle_connection(self):
        if self.btn_connect.isChecked():
            port = self.port_input.text()
            self.serial_thread = SerialThread(port)
            self.serial_thread.samples_ready.connect(self.process_data)
            self.serial_thread.start()
            self.btn_connect.setText("OFF")
        else:
            if self.serial_thread:
                self.serial_thread.stop()
                self.serial_thread = None
            self.btn_connect.setText("ON")

    def process_data(self, samples):
        self.total_samples_received += len(samples)
        now = time.time()
        if now - self.last_time_check >= 1.0:
            self.measured_sps = self.total_samples_received / (now - self.last_time_check)
            self.total_samples_received = 0
            self.last_time_check = now
           # self.plot.setTitle(f"NIBIRU SCOPE - Sampling: {self.measured_sps/1000:.1f} kSPS")

        # Buffer Management
        self.capture_buffer.extend(samples)
        if len(self.capture_buffer) > self.MAX_CAPTURE:
            self.capture_buffer = self.capture_buffer[-self.MAX_CAPTURE:]
        
        # Limitare FPS
        if now - self.last_draw_time < (1.0 / self.FPS_LIMIT):
            return

        
        time_per_div = TIME_STEPS[self.dial_time.value()]
        total_time_screen = time_per_div * 10
        
        n_display_points = int(total_time_screen * self.measured_sps)
        
        n_display_points = max(10, min(n_display_points, len(self.capture_buffer)))
        
        if len(self.capture_buffer) < n_display_points * 2:
            return

    
        search_window = int(n_display_points * 2.5) 
        if search_window > len(self.capture_buffer):
            search_window = len(self.capture_buffer)
            
        raw_data = np.array(self.capture_buffer[-search_window:], dtype=np.int16)

        # Filtru Median (doar daca avem macar 5 puncte)
        if len(raw_data) > 5:
            window = sliding_window_view(raw_data, window_shape=3)
            filtered_data = np.median(window, axis=1)
        else:
            filtered_data = raw_data

        # TRIGGER LOGIC
        trig_adc = int((self.trigger_lvl / 5.0) * 1023)
        
        # Cautare rising edge
        binary = (filtered_data >= trig_adc).astype(np.int8) # binarizam datele filtrate pentru a gasi frontul mai usor
        edges = np.diff(binary) # comparam fiecare punct cu cel dinaintea lui
        trigger_indices = np.where(edges == 1)[0] # indicele din memorie unde a avut loc schimbarea de front
        
        frame_to_draw = None
        found_trigger = False

        if len(trigger_indices) > 0:
            
            for idx in reversed(trigger_indices):
                if idx + n_display_points < len(filtered_data):
                    if np.all(filtered_data[idx+1 : idx+4] > (trig_adc - 20)):
                        frame_to_draw = filtered_data[idx : idx + n_display_points]
                        found_trigger = True
                        
                        self.lbl_trig_status.setText("LOCKED")
                        self.lbl_trig_status.setStyleSheet("color: #00E676; border: 1px solid #00E676; font-weight: bold;")
                        self.trigger_lost_timer = now
                        break
        
        # Auto roll
        if not found_trigger:
            if now - self.trigger_lost_timer > 0.3: 
                self.lbl_trig_status.setText("AUTO")
                self.lbl_trig_status.setStyleSheet("color: orange; border: 1px solid orange;")
                # Luam ultimele date disponibile
                if len(filtered_data) >= n_display_points:
                    frame_to_draw = filtered_data[-n_display_points:]

        # Desenare
        if frame_to_draw is not None:
            self.last_draw_time = now
            
            # Conversie in Volti
            real_volts = (frame_to_draw / 1023.0) * 5.0
            
            display_volts = (real_volts - 2.5) + self.vert_offset
            
            # Generare axa timp
            total_time_screen = time_per_div * 10
            t = np.linspace(0, total_time_screen, len(display_volts))
            
            self.curve.setData(t, display_volts)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())