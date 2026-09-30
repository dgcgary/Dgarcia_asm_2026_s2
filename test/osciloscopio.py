#!/usr/bin/env python3
"""
visualizador de audio en tiempo real para el microfono MAX4466 (GPIO 34)
lee datos del puerto serial y grafica el voltaje de la señal de entrada
"""

import sys
import os
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

PUERTO = 'com3' if sys.platform.startswith('win') else '/dev/ttyUSB0'
MAX_PUNTOS = 250

try:
    import serial
    ser_obj = serial.Serial(PUERTO, 115200, timeout=0.05)
    def leer_linea():
        if ser_obj.in_waiting:
            return ser_obj.readline().decode('utf-8', errors='ignore').strip()
        return None
    print(f"conectado via pyserial a {PUERTO}")
except ImportError:
    os.system(f"stty -F {PUERTO} 115200 cs8 -cstopb -parenb raw -echo 2>/dev/null")
    try:
        ser_fd = open(PUERTO, 'r', encoding='utf-8', errors='ignore', buffering=1)
        def leer_linea():
            import select
            if select.select([ser_fd], [], [], 0.01)[0]:
                return ser_fd.readline().strip()
            return None
        print(f"conectado nativamente a {PUERTO} (115200 baudios)")
    except Exception as e:
        print(f"error al abrir {PUERTO}: {e}")
        print("cierra el monitor serial antes de ejecutar este script.")
        sys.exit(1)

buffer_y = deque([1.65] * MAX_PUNTOS, maxlen=MAX_PUNTOS)
buffer_x = list(range(MAX_PUNTOS))

fig, ax = plt.subplots(figsize=(10, 5))
line, = ax.plot(buffer_x, buffer_y, color='tab:blue', lw=1.5)
ax.set_ylim(0.0, 3.3)
ax.set_title("Osciloscopio en Vivo - Microfono MAX4466 (GPIO 34)", fontsize=12)
ax.set_xlabel("Muestras recientes")
ax.set_ylabel("Voltaje (V)")
ax.grid(True, linestyle='--', alpha=0.6)

def actualizar(frame):
    for _ in range(10):
        linea = leer_linea()
        if not linea:
            break
        if "Offset DC:" in linea:
            try:
                val_str = linea.split("Offset DC:")[1].split("V")[0].strip()
                buffer_y.append(float(val_str))
            except Exception:
                pass
        elif "Vpp (Sonido):" in linea:
            try:
                val_str = linea.split("Vpp (Sonido):")[1].split("V")[0].strip()
                buffer_y.append(float(val_str))
            except Exception:
                pass

    line.set_ydata(buffer_y)
    return line,

ani = animation.FuncAnimation(fig, actualizar, interval=30, blit=False)
plt.tight_layout()
print("Mostrando osciloscopio en pantalla... (cierra la ventana para salir)")
plt.show()
