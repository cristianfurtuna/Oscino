import sys
import time
import serial
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QDoubleSpinBox
)
from PyQt5.QtCore import QThread, pyqtSignal
import pyqtgraph as pg

# --- CONSTANTE (Trebuie să fie identice cu cele din C) ---
SYNC_SEQ = b"\xAA\x55"
BATCH_SIZE = 600      # Corespunde cu #define BATCH_SIZE 200 din main.c
ADC_REF_VOLTAGE = 5.0 # Tensiunea de referință (AVCC)

class SerialADCReader:
    def __init__(self, port="COM5", baudrate=2000000):
        # Timeout mic pentru a nu bloca interfața
        self.ser = serial.Serial(port, baudrate, timeout=0.01)
        self.rx_buf = bytearray()

    def connect(self):
        if not self.ser.is_open:
            self.ser.open()
            self.ser.reset_input_buffer()

    def disconnect(self):
        if self.ser.is_open:
            self.ser.close()

    def read_frame(self):
        # 1. Citim tot ce este disponibil în buffer-ul PC-ului
        if self.ser.in_waiting > 0:
            data = self.ser.read(self.ser.in_waiting)
            self.rx_buf.extend(data)

        frames_found = []
        
        # 2. Procesăm buffer-ul
        while True:
            # Căutăm secvența de sincronizare 0xAA 0x55
            idx = self.rx_buf.find(SYNC_SEQ)
            
            # Dacă nu găsim sync, ieșim
            if idx == -1:
                # Păstrăm doar ultimii bytes pentru a nu umple memoria dacă se pierde sync-ul
                if len(self.rx_buf) > 2 * BATCH_SIZE:
                    self.rx_buf = self.rx_buf[-100:] 
                break
            
            # Verificăm dacă avem tot pachetul (Sync + Date)
            # Avem nevoie de 2 bytes (sync) + BATCH_SIZE bytes (date)
            pkt_len = 2 + BATCH_SIZE
            if len(self.rx_buf) < idx + pkt_len:
                break # Așteptăm să vină restul datelor

            # 3. Extragem datele utile (payload)
            payload = self.rx_buf[idx + 2 : idx + pkt_len]
            
            # --- CONVERSIE 8-BIT ---
            # Datele vin direct ca uint8 (0-255).
            # Folosim numpy pentru viteză maximă.
            raw_data = np.frombuffer(payload, dtype=np.uint8)
            
            # Convertim la int16 și înmulțim cu 4 pentru a simula scala 0-1023
            # (Astfel păstrăm logica de voltaj compatibilă cu ADC-ul de 10 biți)
            scaled_data = raw_data.astype(np.int16) * 4
            
            frames_found.extend(scaled_data)

            # Ștergem pachetul procesat din buffer
            del self.rx_buf[:idx + pkt_len]

        return frames_found if frames_found else None

class SerialThread(QThread):
    samples_ready = pyqtSignal(object)

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
            while self.running:
                samples = self.reader.read_frame()
                if samples:
                    self.samples_ready.emit(samples)
                else:
                    self.msleep(1) # Pauză minusculă pentru a nu bloca CPU-ul
        except Exception as e:
            print(f"Serial Error: {e}")
        finally:
            if self.reader:
                self.reader.disconnect()

    def stop(self):
        self.running = False
        self.wait()

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.serial_thread = None
        
        # Parametri Osciloscop
        self.timebase = 0.002  # 2ms/div (pentru semnale rapide)
        self.volts_per_div = 1.0
        self.horiz_divs = 10
        
        # Buffer circular uriaș (Memorie brută)
        self.capture_buffer = []
        self.MAX_CAPTURE = 30000 
        
        # Câte puncte desenăm pe ecran (Lățimea ferestrei)
        self.N_DISPLAY = 1000    

        # Variabile statistici
        self.total_samples_received = 0
        self.last_time_check = time.time()
        self.measured_sps = 50000.0 

        # --- STABILIZARE VIZUALĂ (Scope Logic) ---
        self.last_draw_time = 0
        self.FPS_LIMIT = 20  # Limităm la 20 FPS pentru stabilitate vizuală
        
        self.trigger_level = 2.5 
        self.last_valid_frame = None # Memorie video pentru "Persistence"

        self._setup_ui()
        self._setup_plot()

    def _setup_ui(self):
        self.port_input = QLineEdit("COM5") 
        self.btn_connect = QPushButton("CONNECT")
        self.btn_connect.clicked.connect(self.toggle_connection)
        self.btn_connect.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")

        self.spin_timebase = QDoubleSpinBox()
        self.spin_timebase.setRange(0.00001, 1.0)
        self.spin_timebase.setValue(self.timebase)
        self.spin_timebase.setDecimals(5)
        self.spin_timebase.setPrefix("T: ")
        self.spin_timebase.setSuffix(" s/div")
        self.spin_timebase.valueChanged.connect(self.update_params)

        self.spin_volts = QDoubleSpinBox()
        self.spin_volts.setRange(0.1, 5.0)
        self.spin_volts.setValue(self.volts_per_div)
        self.spin_volts.setPrefix("V: ")
        self.spin_volts.setSuffix(" V/div")
        self.spin_volts.valueChanged.connect(self.update_params)

        self.lbl_trigger = QLabel("WAITING...")
        self.lbl_trigger.setStyleSheet("color: gray; font-weight: bold;")

        ctrl_layout = QVBoxLayout()
        ctrl_layout.addWidget(QLabel("Serial Port:"))
        ctrl_layout.addWidget(self.port_input)
        ctrl_layout.addWidget(self.btn_connect)
        ctrl_layout.addWidget(QLabel("Timebase:"))
        ctrl_layout.addWidget(self.spin_timebase)
        ctrl_layout.addWidget(QLabel("Voltage:"))
        ctrl_layout.addWidget(self.spin_volts)
        ctrl_layout.addWidget(self.lbl_trigger)
        ctrl_layout.addStretch()

        self.plot_widget = pg.GraphicsLayoutWidget()
        self.plot = self.plot_widget.addPlot(title="Nibiru Scope - Stable Lock")
        self.curve = self.plot.plot(pen=pg.mkPen('#00FF00', width=2)) 
        
        # Grid static
        self.plot.showGrid(x=True, y=True, alpha=0.5)

        main_layout = QHBoxLayout()
        main_layout.addLayout(ctrl_layout, 1)
        main_layout.addWidget(self.plot_widget, 4)

        container = QWidget()
        container.setLayout(main_layout)
        self.setCentralWidget(container)
        self.setWindowTitle("Nibiru Scope")
        self.resize(1100, 600)

    def _setup_plot(self):
        pg.setConfigOptions(antialias=False) 
        self.plot.setLabel('bottom', 'Time (s)')
        self.plot.setLabel('left', 'Voltage (V)')
        self.plot.setYRange(-0.5, 5.5)
        self.update_params()

    def update_params(self):
        self.timebase = self.spin_timebase.value()
        self.volts_per_div = self.spin_volts.value()
        # Axa X este fixă (0 -> Timebase * 10)
        t_window = self.timebase * self.horiz_divs
        self.plot.setXRange(0, t_window)

    def toggle_connection(self):
        if self.serial_thread is None:
            port = self.port_input.text()
            self.serial_thread = SerialThread(port)
            self.serial_thread.samples_ready.connect(self.process_data)
            self.serial_thread.start()
            self.btn_connect.setText("DISCONNECT")
            self.btn_connect.setStyleSheet("background-color: #F44336; color: white;")
        else:
            self.serial_thread.stop()
            self.serial_thread = None
            self.btn_connect.setText("CONNECT")
            self.btn_connect.setStyleSheet("background-color: #4CAF50; color: white;")

    def process_data(self, samples):
        # 1. Statistici
        self.total_samples_received += len(samples)
        now = time.time()
        if now - self.last_time_check >= 1.0:
            self.measured_sps = self.total_samples_received / (now - self.last_time_check)
            self.total_samples_received = 0
            self.last_time_check = now
            self.setWindowTitle(f"Nibiru Scope - {self.measured_sps/1000:.1f} kSPS")

        # 2. Buffer
        self.capture_buffer.extend(samples)
        if len(self.capture_buffer) > self.MAX_CAPTURE:
            self.capture_buffer = self.capture_buffer[-self.MAX_CAPTURE:]
        
        # --- HOLD-OFF ---
        if now - self.last_draw_time < (1.0 / self.FPS_LIMIT):
            return
        
        # Avem nevoie de date
        required_samples = self.N_DISPLAY * 2
        if len(self.capture_buffer) < required_samples:
            return

        # 3. PRELUCRARE & FILTRARE
        raw_data = np.array(self.capture_buffer[-required_samples:], dtype=np.int16)
        
        # Filtru Median (Anti-Spike)
        window = sliding_window_view(raw_data, window_shape=3)
        filtered_data = np.median(window, axis=1)

        # 4. CĂUTARE TRIGGER
        trig_val = int((self.trigger_level / 5.0) * 1023)
        binary = (filtered_data >= trig_val).astype(np.int8)
        edges = np.diff(binary) 
        trigger_indices = np.where(edges == 1)[0]

        frame_to_draw = None
        found_trigger = False

        if len(trigger_indices) > 0:
            # Căutăm primul trigger stabil (Forward search)
            for idx in trigger_indices:
                if idx + self.N_DISPLAY < len(filtered_data):
                    # Noise Check (5 puncte stabile)
                    if np.all(filtered_data[idx+1 : idx+6] > (trig_val - 20)):
                        frame_to_draw = filtered_data[idx : idx + self.N_DISPLAY]
                        found_trigger = True
                        self.lbl_trigger.setText("LOCKED")
                        self.lbl_trigger.setStyleSheet("color: #00FF00; font-weight: bold;")
                        break 

        # 5. LOGICA AUTO-ROLL (Dacă nu am găsit trigger)
        if not found_trigger:
            # Verificăm cât timp a trecut de la ultimul desen
            time_since_last_draw = now - self.last_draw_time
            
            if time_since_last_draw > 0.2: # Timeout de 200ms
                # Dacă am așteptat prea mult, trecem pe AUTO
                self.lbl_trigger.setText("AUTO (DC/No Trigger)")
                self.lbl_trigger.setStyleSheet("color: orange; font-weight: bold;")
                
                # Luăm pur și simplu ultimele N_DISPLAY date filtrate
                if len(filtered_data) >= self.N_DISPLAY:
                    frame_to_draw = filtered_data[-self.N_DISPLAY:]

        # 6. DESENARE
        if frame_to_draw is not None:
            self.last_valid_frame = frame_to_draw
            self.last_draw_time = now # Resetăm timerul doar dacă desenăm
            
            volts = (frame_to_draw / 1023.0) * 5.0
            total_time = self.timebase * self.horiz_divs
            t = np.linspace(0, total_time, len(volts))
            self.curve.setData(t, volts)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())