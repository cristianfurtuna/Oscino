# import serial
# import numpy as np
# import pyqtgraph as pg
# from pyqtgraph.Qt import QtWidgets, QtCore
# import sys

# # ------------------------------
# # Serial Config
# # ------------------------------
# ser = serial.Serial("COM5", 2000000, timeout=0)

# SYNC = b"\xAA\x55"
# SYNC_LEN = 2
# SAMPLES_PER_BATCH = 64
# BITS_PER_SAMPLE = 10
# BYTES_PER_BATCH = (SAMPLES_PER_BATCH * BITS_PER_SAMPLE) // 8  # = 80

# WINDOW = 2000  # plotted window size (averaged points)
# data_window = np.zeros(WINDOW, dtype=np.uint16)
# write_index = 0

# bit_buffer = 0
# bits_in_buffer = 0
# buffer = bytearray()

# # Averaging factor: tune this for responsiveness vs detail
# AVG_FACTOR = 200  # average every 200 samples → ~1 ksps plotted
# avg_accum = []

# # ------------------------------
# # Bit unpacking
# # ------------------------------
# def unpack_bits(bytes_in):
#     global bit_buffer, bits_in_buffer
#     samples = []
#     for byte in bytes_in:
#         bit_buffer |= (byte << bits_in_buffer)
#         bits_in_buffer += 8
#         while bits_in_buffer >= 10:
#             samples.append(bit_buffer & 0x3FF)
#             bit_buffer >>= 10
#             bits_in_buffer -= 10
#     return samples

# # ------------------------------
# # PyQtGraph Setup
# # ------------------------------
# app = QtWidgets.QApplication(sys.argv)
# pg.setConfigOptions(antialias=True, background="w", foreground="k")

# win = pg.GraphicsLayoutWidget(show=True, title="Live MCP3008 Data")
# win.resize(900, 500)
# plot = win.addPlot(title="ADC Live Stream")
# curve = plot.plot(pen=pg.mkPen(width=2))

# plot.setYRange(0, 1023)
# plot.setXRange(0, WINDOW)

# # ------------------------------
# # Update function
# # ------------------------------
# def update():
#     global buffer, write_index, data_window, avg_accum

#     samples = []

#     buffer.extend(ser.read(16384))

#     # Drop backlog if too large
#     if len(buffer) > 16384 * 4:
#         buffer.clear()

#     # Process only the latest batch
#     while len(buffer) >= SYNC_LEN + BYTES_PER_BATCH:
#         idx = buffer.find(SYNC)
#         if idx == -1:
#             break
#         del buffer[:idx + SYNC_LEN]
#         if len(buffer) < BYTES_PER_BATCH:
#             break
#         batch = buffer[:BYTES_PER_BATCH]
#         del buffer[:BYTES_PER_BATCH]
#         samples = unpack_bits(batch)

#     # Only use the last batch's samples for plotting
#     if not samples:
#         return

#     # Average groups of samples
#     for s in samples:
#         avg_accum.append(s)
#         if len(avg_accum) == AVG_FACTOR:
#             avg_val = int(np.mean(avg_accum))
#             avg_accum.clear()
#             data_window[write_index] = avg_val
#             write_index = (write_index + 1) % WINDOW

#     # Plot newest averaged data
#     curve.setData(np.roll(data_window, -write_index))

# # ------------------------------
# # Timer for updates
# # ------------------------------
# timer = QtCore.QTimer()
# timer.timeout.connect(update)
# timer.start(0)

# # ------------------------------
# # Start
# # ------------------------------
# if __name__ == "__main__":
#     sys.exit(app.exec_())

import serial
import numpy as np
import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets, QtCore
import sys, time

# ------------------------------
# Serial Config
# ------------------------------
ser = serial.Serial("COM5", 2000000, timeout=0)

SYNC = b"\xAA\x55"
SYNC_LEN = 2
SAMPLES_PER_BATCH = 64
BITS_PER_SAMPLE = 10
BYTES_PER_BATCH = (SAMPLES_PER_BATCH * BITS_PER_SAMPLE) // 8  # = 80

# ------------------------------
# Plotting Config
# ------------------------------
SAMPLE_RATE = 200000       # ADC samples/sec
AVG_FACTOR  = 200          # samples averaged into one plotted point
DT          = AVG_FACTOR / SAMPLE_RATE  # seconds per plotted point

WINDOW_POINTS = 2000       # number of averaged points stored
TIME_SPAN    = WINDOW_POINTS * DT  # seconds of history shown

voltage_window = np.zeros(WINDOW_POINTS, dtype=np.float32)
time_window    = np.zeros(WINDOW_POINTS, dtype=np.float32)
write_index    = 0

bit_buffer = 0
bits_in_buffer = 0
buffer = bytearray()
avg_accum = []

start_time = time.time()

# ------------------------------
# Bit unpacking
# ------------------------------
def unpack_bits(bytes_in):
    global bit_buffer, bits_in_buffer
    samples = []
    for byte in bytes_in:
        bit_buffer |= (byte << bits_in_buffer)
        bits_in_buffer += 8
        while bits_in_buffer >= 10:
            samples.append(bit_buffer & 0x3FF)
            bit_buffer >>= 10
            bits_in_buffer -= 10
    return samples

# ------------------------------
# PyQtGraph Setup
# ------------------------------
app = QtWidgets.QApplication(sys.argv)
pg.setConfigOptions(antialias=True, background="w", foreground="k")

win  = pg.GraphicsLayoutWidget(show=True, title="Live MCP3008 Voltage")
win.resize(900, 500)
plot = win.addPlot(title="Voltage vs Time (Real)")
curve = plot.plot(pen=pg.mkPen(width=2))

plot.setYRange(0, 5.0)

# ------------------------------
# Update function
# ------------------------------
def update():
    global buffer, write_index, voltage_window, time_window, avg_accum

    samples = []

    buffer.extend(ser.read(16384))

    # Drop backlog if too large
    if len(buffer) > 16384 * 4:
        buffer.clear()

    # Consume to the newest batch
    while len(buffer) >= SYNC_LEN + BYTES_PER_BATCH:
        idx = buffer.find(SYNC)
        if idx == -1:
            break
        del buffer[:idx + SYNC_LEN]
        if len(buffer) < BYTES_PER_BATCH:
            break
        batch = buffer[:BYTES_PER_BATCH]
        del buffer[:BYTES_PER_BATCH]
        samples = unpack_bits(batch)

    if not samples:
        return

    # Average and convert to voltage
    for s in samples:
        avg_accum.append(s)
        if len(avg_accum) == AVG_FACTOR:
            avg_val = np.mean(avg_accum)
            avg_accum.clear()
            voltage = (avg_val / 1023.0) * 5.0
            elapsed = time.time() - start_time
            voltage_window[write_index] = voltage
            time_window[write_index]    = elapsed
            write_index = (write_index + 1) % WINDOW_POINTS

    # Roll buffers so newest data is at the end
    x = np.roll(time_window, -write_index)
    y = np.roll(voltage_window, -write_index)

    # Plot latest data
    curve.setData(x, y)

    # Keep X-axis showing last TIME_SPAN seconds
    if x[-1] > 0:
        plot.setXRange(x[-1] - TIME_SPAN, x[-1])

# ------------------------------
# Timer for updates
# ------------------------------
timer = QtCore.QTimer()
timer.timeout.connect(update)
timer.start(0)

# ------------------------------
# Start
# ------------------------------
if __name__ == "__main__":
    sys.exit(app.exec_())



#Program test afisare biti receptionati in consola



# import serial

# ser = serial.Serial('COM5', 2000000, timeout=1)

# # Persistent buffers
# bit_buffer = 0
# bits_in_buffer = 0

# def unpack_data(data_bytes, bit_buffer=0, bits_in_buffer=0):
#     samples = []
#     for byte in data_bytes:
#         bit_buffer |= (byte << bits_in_buffer)
#         bits_in_buffer += 8
#         while bits_in_buffer >= 10:
#             samples.append(bit_buffer & 0x3FF)  # get 10-bit sample
#             bit_buffer >>= 10
#             bits_in_buffer -= 10
#     return samples, bit_buffer, bits_in_buffer

# while True:
#     data = ser.read(1024)
#     if not data:
#         continue

#     print("Received", len(data), "bytes")
#     print(data[:20])  # first 20 bytes for inspection

#     samples, bit_buffer, bits_in_buffer = unpack_data(data, bit_buffer, bits_in_buffer)

#     if samples:
#         print("Unpacked samples:", samples[:10])  # first 10 samples

