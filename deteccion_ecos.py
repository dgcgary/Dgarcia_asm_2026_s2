"""
CE1110 - Análisis de Señales Mixtas
Taller Semana 5: Módulo de Detección de Ecos por Correlación Cruzada.
"""

import os
import numpy as np

from experimentos_fft import fft  # <-- FFT propia (Cooley-Tukey), implementada en la Parte 2

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


# --------------------------------------------------------------------------
# A partir de aqui se construye la correlacion via FFT usando nuestra
# implementacion de Cooley-Tukey.
# No se usa np.fft en ningun punto de este archivo.
# --------------------------------------------------------------------------

def siguiente_potencia_dos(n):
    """
    Retorna la potencia de 2 mas pequena que es >= n.

    Es necesaria porque el algoritmo Radix-2 de Cooley-Tukey (experimentos_fft.fft)
    solo funciona correctamente cuando el largo de la señal es una
    potencia de 2 (se va dividiendo a la mitad en cada nivel de la
    recursion). Cualquier otro largo se debe rellenar con ceros
    (zero-padding) hasta la siguiente potencia de 2.
    """
    if n <= 1:
        return 1
    return 1 << (n - 1).bit_length()


def ifft_propia(X):
    """
    Transformada inversa de Fourier (IFFT), reutilizando nuestra
    'fft' (Cooley-Tukey) mediante la identidad:

        ifft(X) = conj( fft( conj(X) ) ) / N

    Esto evita tener que programar una segunda version de Cooley-Tukey
    para la transformada inversa: basta con conjugar la entrada, aplicar
    la misma FFT hacia adelante, conjugar de nuevo el resultado y
    dividir entre N.

    Parametros
    ----------
    X : list[complex]  (largo N, potencia de 2)

    Retorna
    -------
    list[complex] de largo N
    """
    N = len(X)
    X_conjugado = [muestra.conjugate() for muestra in X]
    x = fft(X_conjugado)
    return [muestra.conjugate() / N for muestra in x]


def correlacion_fft(y, x):
    """
    Calcula la correlación cruzada en el dominio de la frecuencia usando
    nuestra FFT, aplicando el teorema de
    convolución:

        corr(y, x)[m] = IFFT( FFT(y) * conj(FFT(x)) )

    Complejidad: O(L log L), donde L es la longitud usada para la FFT.

    Detalles de implementación:
    1) El algoritmo Radix-2 exige que el largo de la señal sea potencia
       de 2, así que se rellena (zero-padding) hasta la siguiente
       potencia de 2 que sea >= N + M - 1.
    2) El "+ M - 1" (en vez de solo N) es indispensable para que la FFT
       calcule la correlación LINEAL (la que tiene sentido físico aquí)
       y no la correlación CIRCULAR, que es lo que se obtendría si no
       se rellenara lo suficiente (efecto de "wrap-around"/aliasing).

    Parámetros
    ----------
    y : np.ndarray   (largo N) señal recibida
    x : np.ndarray   (largo M) señal conocida (plantilla)

    Retorna
    -------
    R : np.ndarray (largo N-M+1) correlación cruzada, recortada para
        ser directamente comparable con correlacion_directa().
    """
    N = len(y)
    M = len(x)
    L_minimo = N + M - 1
    L = siguiente_potencia_dos(L_minimo)

    # Zero-padding: la funcion fft() trabaja con listas,
    # por eso se convierte de arreglo de numpy a lista de Python.
    y_pad = list(y) + [0.0] * (L - N)
    x_pad = list(x) + [0.0] * (L - M)

    Y = fft(y_pad)
    X = fft(x_pad)

    # Multiplicacion en frecuencia por el conjugado (teorema de convolucion)
    producto = [Y[k] * X[k].conjugate() for k in range(L)]

    R_completa = ifft_propia(producto)

    # Nos quedamos solo con los desplazamientos "validos" (0..N-M), que
    # es la misma region que entrega correlacion_directa(). Se toma la
    # parte real porque, en teoria, la correlacion de señales reales es
    # real; cualquier parte imaginaria remanente es error numerico de
    # punto flotante.
    R = np.array([muestra.real for muestra in R_completa[: N - M + 1]])
    return R


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
    print("Correlación via FFT usa la implementación de Cooley-Tukey")
    print("(experimentos_fft.fft), NO se usa np.fft en ningún punto de este archivo.")