#!/usr/bin/env python3
"""
============================================================================
CE1110 - Osciloscopio Serial en Tiempo Real para Arch Linux (Micrófono GPIO 34)
============================================================================
Lee directamente de /dev/ttyUSB0 y grafica en tiempo real sin dependencias externas.
"""

import sys
import os
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

PUERTO = 'com3' if sys.platform.startswith('win') else '/dev/ttyUSB0'
MAX_PUNTOS = 250

# Intentar usar pyserial si está instalado, si no, usar lectura nativa de Linux
try:
    import serial
    ser_obj = serial.Serial(PUERTO, 115200, timeout=0.05)
    def leer_linea():
        if ser_obj.in_waiting:
            return ser_obj.readline().decode('utf-8', errors='ignore').strip()
        return None
    print(f"[OK] Conectado vía pyserial a {PUERTO}.")
except ImportError:
    # Configurar baudrate a 115200 con stty de Linux
    os.system(f"stty -F {PUERTO} 115200 cs8 -cstopb -parenb raw -echo 2>/dev/null")
    try:
        ser_fd = open(PUERTO, 'r', encoding='utf-8', errors='ignore', buffering=1)
        def leer_linea():
            import select
            if select.select([ser_fd], [], [], 0.01)[0]:
                return ser_fd.readline().strip()
            return None
        print(f"[OK] Conectado nativamente a {PUERTO} (115200 baudios).")
    except Exception as e:
        print(f"[ERROR] No se pudo abrir {PUERTO}: {e}")
        print("Asegúrate de que el monitor serial de VS Code esté cerrado.")
        sys.exit(1)

buffer_y = deque([1.34] * MAX_PUNTOS, maxlen=MAX_PUNTOS)
buffer_x = list(range(MAX_PUNTOS))

fig, ax = plt.subplots(figsize=(10, 5))
line, = ax.plot(buffer_x, buffer_y, color='tab:blue', lw=1.5)
ax.set_ylim(0.0, 3.3)
ax.set_title("Osciloscopio en Vivo - Micrófono MAX4466 (GPIO 34)", fontsize=12)
ax.set_xlabel("Muestras recientes")
ax.set_ylabel("Voltaje (V)")
ax.axhline(1.34, color='red', linestyle='--', alpha=0.5, label='Offset DC (~1.34V)')
ax.grid(True, linestyle='--', alpha=0.6)
ax.legend(loc='upper right')

def actualizar(frame):
    for _ in range(10): # Leer ráfagas de muestras disponibles
        linea = leer_linea()
        if not linea:
            break
        if "Voltaje:" in linea:
            try:
                val_str = linea.split("Voltaje:")[1].split("V")[0].strip()
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

ani = animation.FuncAnimation(fig, actualizar, interval=30, blit=True)
plt.tight_layout()
print("Mostrando osciloscopio en pantalla... (Cierra la ventana para salir)")
plt.show()
