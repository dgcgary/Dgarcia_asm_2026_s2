#!/usr/bin/env python3
"""
diagnostico grafico del radar acustico (test 6)
recibe tramas JSON emitidas por el ESP32 y grafica en tiempo real:
1. señal transmitida (chirp de referencia)
2. señal recibida en el microfono (cruda vs filtrada con biquad)
3. perfil de correlacion cruzada vs distancia en cm, marcando la zona ciega y el pico
"""

import sys
import os
import json
import time
import glob
import numpy as np
import matplotlib.pyplot as plt

BAUDIOS = 115200

def detectar_puerto():
    if len(sys.argv) > 1:
        return sys.argv[1]
    
    # deteccion en Linux
    puertos_linux = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
    if puertos_linux:
        return puertos_linux[0]
    
    # deteccion en Windows
    if sys.platform.startswith('win'):
        import serial.tools.list_ports
        puertos_win = [p.device for p in serial.tools.list_ports.comports()]
        if puertos_win:
            return puertos_win[0]
    
    return '/dev/ttyUSB0'

PUERTO = detectar_puerto()
print(f"conectando al puerto: {PUERTO} a {BAUDIOS} baudios...")

try:
    import serial
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=0.5)
    time.sleep(1.0)
    print(f"conectado exitosamente via PySerial a {PUERTO}")
except ImportError:
    print("PySerial no detectado, usa lectura nativa POSIX...")
    os.system(f"stty -F {PUERTO} {BAUDIOS} cs8 -cstopb -parenb raw -echo 2>/dev/null")
    try:
        ser = open(PUERTO, 'r', encoding='utf-8', errors='ignore', buffering=1)
        print(f"conectado nativamente a {PUERTO}")
    except Exception as e:
        print(f"error al abrir {PUERTO}: {e}")
        print("cierra el monitor serial antes de ejecutar este script.")
        sys.exit(1)
except Exception as e:
    print(f"error al abrir {PUERTO}: {e}")
    print("verifica permisos o cierra otros programas que usen el puerto.")
    sys.exit(1)

# configuracion de la ventana grafica
plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
fig = plt.figure(figsize=(13, 8))
fig.canvas.manager.set_window_title("CE1110 - Diagnostico en Vivo del Radar Acustico (Test 6)")
gs = fig.add_gridspec(3, 1, height_ratios=[1, 1.2, 1.8], hspace=0.35)

ax_tx = fig.add_subplot(gs[0])
ax_rx = fig.add_subplot(gs[1])
ax_corr = fig.add_subplot(gs[2])

def procesar_linea(linea):
    linea = linea.strip()
    if not linea.startswith('{"tipo":"radar6"'):
        return None
    try:
        return json.loads(linea)
    except Exception as e:
        print(f"aviso: error al procesar JSON: {e}")
        return None

def leer_siguiente_disparo():
    while True:
        if hasattr(ser, 'readline'):
            raw = ser.readline()
            if isinstance(raw, bytes):
                linea = raw.decode('utf-8', errors='ignore').strip()
            else:
                linea = raw.strip()
        else:
            linea = ser.readline().strip()
        
        if not linea:
            plt.pause(0.02)
            continue
        
        if linea.startswith("TEST 6:") or linea.startswith("CE1110:"):
            print(f"[ESP32] {linea}")
            continue
        
        datos = procesar_linea(linea)
        if datos:
            return datos

print("\nSistema de diagnostico grafico activo (Test 6)")
print("Esperando lecturas del ESP32...\n")

plt.ion()
plt.show()

try:
    while plt.fignum_exists(fig.number):
        d = leer_siguiente_disparo()
        if not d:
            continue

        fs_real = d.get("fs_real", 10000.0)
        vel_sonido = d.get("vel_sonido", 343.0)
        zona_ciega = d.get("zona_ciega", 22)
        umbral = d.get("umbral", 500.0)
        max_dist_cm = d.get("max_distancia_cm", 250.0)
        pico_idx = d.get("pico", 0)
        dist_cm = d.get("distancia_cm", 0.0)
        tof_ms = d.get("tof_ms", 0.0)
        amplitud = d.get("amplitud", 0.0)
        detectado = d.get("detectado", False)
        saturadas = d.get("saturadas", 0)
        dsp_us = d.get("dsp_us", 0)
        captura_us = d.get("captura_us", 0)

        tx = np.array(d.get("tx", []))
        rx = np.array(d.get("rx", []))
        rx_f = np.array(d.get("rx_filtrada", []))
        corr = np.array(d.get("corr", []))
        corr_f = np.array(d.get("corr_filtrada", []))

        t_tx_ms = np.arange(len(tx)) / fs_real * 1000.0
        t_rx_ms = np.arange(len(rx)) / fs_real * 1000.0

        # eje de distancia: d = (vs * m / fs) / 2 * 100
        m_indices = np.arange(len(corr_f))
        dist_eje_cm = (vel_sonido * (m_indices / fs_real) / 2.0) * 100.0

        # panel 1: chirp transmitido
        ax_tx.clear()
        ax_tx.plot(t_tx_ms, tx, color='tab:green', lw=1.5, label='Chirp Tx (Referencia)')
        ax_tx.set_title(f"1. Señal Transmitida (Chirp {d.get('f0_nominal',1000):.0f} Hz -> {d.get('f1_nominal',3000):.0f} Hz) | Duracion: {len(tx)} muestras ({len(tx)/fs_real*1000:.2f} ms)", fontsize=10, fontweight='bold')
        ax_tx.set_xlabel("Tiempo [ms]", fontsize=9)
        ax_tx.set_ylabel("Amplitud Normalizada", fontsize=9)
        ax_tx.grid(True, linestyle='--', alpha=0.5)
        ax_tx.set_ylim(-1.1, 1.1)
        ax_tx.legend(loc='upper right', fontsize=8)

        # panel 2: señal recibida en microfono
        ax_rx.clear()
        ax_rx.plot(t_rx_ms, rx, color='gray', alpha=0.5, lw=1.0, label='Rx Cruda (sin DC)')
        ax_rx.plot(t_rx_ms, rx_f, color='tab:blue', lw=1.3, label='Rx Filtrada (Biquad Pasa Banda)')
        ax_rx.set_title(f"2. Señal Capturada por Microfono | Ventana: {len(rx)} muestras ({captura_us/1000:.2f} ms) | Offset DC: {d.get('media_adc', 0):.1f} | Saturadas: {saturadas}", fontsize=10, fontweight='bold')
        ax_rx.set_xlabel("Tiempo [ms]", fontsize=9)
        ax_rx.set_ylabel("Amplitud ADC", fontsize=9)
        ax_rx.grid(True, linestyle='--', alpha=0.5)
        ax_rx.legend(loc='upper right', fontsize=8)

        # panel 3: correlacion cruzada y deteccion de eco
        ax_corr.clear()
        abs_corr_f = np.abs(corr_f)
        ax_corr.plot(dist_eje_cm, abs_corr_f, color='tab:purple', lw=1.6, label='|Correlacion Cruzada (FFT)|')

        # sombrea la zona ciega
        dist_zona_ciega_cm = (vel_sonido * (zona_ciega / fs_real) / 2.0) * 100.0
        ax_corr.axvspan(0, dist_zona_ciega_cm, color='tab:red', alpha=0.15, label=f'Zona Ciega ({zona_ciega} mstras ≈ {dist_zona_ciega_cm:.1f} cm)')
        ax_corr.axvline(dist_zona_ciega_cm, color='tab:red', linestyle=':', lw=1.2)

        # linea de umbral
        ax_corr.axhline(umbral, color='tab:orange', linestyle='--', lw=1.2, label=f'Umbral Minimo ({umbral:.0f})')

        # marca el pico detectado
        if detectado and pico_idx < len(dist_eje_cm):
            pico_dist = dist_eje_cm[pico_idx]
            pico_amp = abs_corr_f[pico_idx]
            ax_corr.plot(pico_dist, pico_amp, marker='*', markersize=14, color='red', label=f'Eco Principal: {dist_cm:.1f} cm (tau = {tof_ms:.2f} ms)')
            ax_corr.annotate(
                f" ECO: {dist_cm:.1f} cm\n ToF: {tof_ms:.2f} ms\n Amp: {pico_amp:.0f}",
                xy=(pico_dist, pico_amp),
                xytext=(pico_dist + 8, pico_amp * 0.85),
                arrowprops=dict(facecolor='red', shrink=0.08, width=1.5, headwidth=6),
                bbox=dict(boxstyle="round,pad=0.3", fc="yellow", alpha=0.7),
                fontweight='bold', fontsize=9
            )
            titulo_estado = f"3. Correlacion Cruzada -> [ECO DETECTADO] Distancia: {dist_cm:.1f} cm | ToF: {tof_ms:.2f} ms | DSP: {dsp_us} us"
            ax_corr.set_title(titulo_estado, fontsize=11, fontweight='bold', color='darkgreen')
        else:
            titulo_estado = f"3. Correlacion Cruzada -> [BUSCANDO / SIN ECO VALIDO] Max Amp: {amplitud:.1f} (Umbral: {umbral:.0f}) | DSP: {dsp_us} us"
            ax_corr.set_title(titulo_estado, fontsize=11, fontweight='bold', color='darkred')

        ax_corr.set_xlabel("Distancia Estimada al Objeto [cm]", fontsize=10, fontweight='bold')
        ax_corr.set_ylabel("Magnitud de Correlacion", fontsize=9)
        ax_corr.set_xlim(0, max_dist_cm)
        ax_corr.grid(True, linestyle='--', alpha=0.5)
        ax_corr.legend(loc='upper right', fontsize=8)

        fig.canvas.draw_idle()
        plt.pause(0.01)

        if detectado:
            print(f"[Disparo #{d.get('id',0):3d}] -> DISTANCIA: {dist_cm:6.1f} cm | ToF: {tof_ms:5.2f} ms | Pico (m): {pico_idx:3d} | Amp: {amplitud:7.1f} | fs_real: {fs_real:.0f} Hz")
        else:
            print(f"[Disparo #{d.get('id',0):3d}] -> Sin eco valido (Amp maxima: {amplitud:6.1f} < Umbral: {umbral:.0f}) | fs_real: {fs_real:.0f} Hz")

except KeyboardInterrupt:
    print("\nDiagnostico detenido por el usuario.")
finally:
    if hasattr(ser, 'close'):
        ser.close()
    print("Puerto serial cerrado.")
