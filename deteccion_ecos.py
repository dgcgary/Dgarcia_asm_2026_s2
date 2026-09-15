"""
CE1110 - Análisis de Señales Mixtas
Taller Semana 5: Módulo de Detección de Ecos por Correlación Cruzada.
"""

import os
import numpy as np

# Semilla fija para asegurar repetibilidad en las pruebas
np.random.seed(42)

CARPETA_SALIDA = "figuras"
os.makedirs(CARPETA_SALIDA, exist_ok=True)


def generar_chirp(fs, duracion, f0, f1):
    """
    Genera un pulso chirp lineal con ventana Hanning aplicada.
    
    El barrido en frecuencia genera una autocorrelación con un pico angosto,
    lo que permite alta resolución temporal al detectar el eco.
    """
    t = np.arange(0, duracion, 1 / fs)
    k = (f1 - f0) / duracion  # Tasa de barrido (chirp rate)
    fase = 2 * np.pi * (f0 * t + 0.5 * k * t**2)
    x = np.sin(fase)

    # Ventana Hanning para suavizar bordes y evitar fugas espectrales
    ventana = np.hanning(len(x))
    return t, x * ventana


def generar_senal_recibida(x, fs, retardos_s, atenuaciones, ruido_std, duracion_total_s):
    """
    Simula la señal capturada por el micrófono: suma de ecos atenuados,
    retardados y contaminación con ruido blanco gaussiano.
    """
    N_total = int(duracion_total_s * fs)
    y = np.zeros(N_total)
    retardos_muestras = []

    for retardo_s, atenuacion in zip(retardos_s, atenuaciones):
        n0 = int(round(retardo_s * fs))
        retardos_muestras.append(n0)
        n1 = n0 + len(x)
        
        # Ajustar si el eco excede el límite de la ventana de recepción
        x_recortada = x[: N_total - n0] if n1 > N_total else x
        y[n0 : n0 + len(x_recortada)] += atenuacion * x_recortada

    # Ruido gaussiano ambiental / ADC
    ruido = np.random.normal(0, ruido_std, N_total)
    y += ruido

    t = np.arange(N_total) / fs
    return t, y, retardos_muestras


def correlacion_directa(y, x):
    """
    Calcula la correlación cruzada mediante definición directa en el dominio del tiempo.
    
    Complejidad: O(N * M). Desliza la plantilla 'x' sobre 'y' realizando
    producto punto muestra a muestra.
    """
    N, M = len(y), len(x)
    n_desplazamientos = N - M + 1
    R = np.zeros(n_desplazamientos)

    for m in range(n_desplazamientos):
        R[m] = np.dot(y[m : m + M], x)

    return R


def correlacion_fft(y, x):
    """
    Calcula la correlación cruzada en el dominio de la frecuencia mediante FFT.
    
    Complejidad: O(L log L). Aplica el teorema de convolución multiplicando
    el espectro de 'y' por el conjugado del espectro de 'x'. Usa zero-padding
    para evitar aliasing circular (correlación lineal).
    """
    N, M = len(y), len(x)
    L = N + M - 1  # Longitud con zero-padding para correlación lineal

    Y = np.fft.fft(y, n=L)
    X = np.fft.fft(x, n=L)

    R_completa = np.fft.ifft(Y * np.conj(X)).real
    return R_completa[: N - M + 1]


def estimar_retardo(R, fs):
    """
    Estima la muestra y el tiempo del pico principal de correlación (un solo eco).
    """
    n_pico = int(np.argmax(R))
    t_pico = n_pico / fs
    return n_pico, t_pico


def encontrar_picos(R, fs, umbral_relativo=0.5, distancia_min_muestras=50):
    """
    Detecta múltiples picos de correlación correspondientes a varios objetos/ecos.
    """
    R_abs = np.abs(R)
    umbral = umbral_relativo * np.max(R_abs)
    candidatos = np.where(R_abs >= umbral)[0]

    picos = []
    for idx in candidatos:
        if all(abs(idx - p) >= distancia_min_muestras for p in picos):
            picos.append(idx)

    # Refinamiento al máximo local dentro de cada vecindad
    picos_finales = []
    for p in picos:
        ventana = range(
            max(0, p - distancia_min_muestras // 2),
            min(len(R), p + distancia_min_muestras // 2),
        )
        p_real = max(ventana, key=lambda i: R_abs[i])
        picos_finales.append(p_real)

    picos_finales = sorted(set(picos_finales))
    tiempos = [p / fs for p in picos_finales]
    return picos_finales, tiempos


if __name__ == "__main__":
    print("Módulo de funciones de detección de ecos cargado.")