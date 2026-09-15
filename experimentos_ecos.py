"""
Este script usa las funciones de 'deteccion_ecos.py' para correr 4
experimentos y generar las imagenes que se deben incluir en el documento
tecnico:

    Experimento 1: Caso base (un eco, poco ruido)
                    -> senal Tx, senal Rx, correlacion directa vs FFT
    Experimento 2: Multiples ecos (varios "objetos")
                    -> deteccion de varios picos de correlacion
    Experimento 3: Robustez ante ruido
                    -> como se degrada la deteccion al subir el ruido
    Experimento 4: Comparacion de tiempo de ejecucion
                    -> directa (O(N*M)) vs FFT (O(N log N))

Cada figura se guarda en la carpeta 'figuras/'.
==========================================================================
"""

import numpy as np
import matplotlib.pyplot as plt
import time

from deteccion_ecos import (
    generar_chirp,
    generar_senal_recibida,
    correlacion_directa,
    correlacion_fft,
    estimar_retardo,
    encontrar_picos,
    CARPETA_SALIDA,
)

# --------------------------------------------------------------------
# Parametros generales del "radar acustico" simulado
# --------------------------------------------------------------------
#
# JUSTIFICACION DE CADA PARAMETRO:
#
# FS = 44100 Hz
#   Es la frecuencia de muestreo estandar de audio. Con f1 = 8000 Hz, el criterio
#   de Nyquist solo exige fs > 16000 Hz; usamos 44100 Hz para tener un
#   margen amplio (~2.75x el minimo) que absorbe imperfecciones reales
#   (roll-off de filtros anti-aliasing, ruido de cuantizacion, etc.).
#
# F0, F1 = 2000, 8000 Hz (banda del chirp)
#   Se eligio una banda AUDIBLE (no ultrasonica) porque es la que puede
#   reproducir/capturar hardware simple y barato (parlante pequeno +
#   microfono electret). Se evitan frecuencias muy bajas (<1 kHz)
#   porque un parlante/tweeter pequeno las reproduce mal, y se evitan
#   frecuencias >10 kHz para dar margen de sobra respecto a Nyquist.
#   (Nota para el documento: si se quisiera evitar interferencia con
#   ruido audible ambiental, la alternativa de diseno seria migrar a
#   una banda ultrasonica ~40 kHz, como el HC-SR04, a costa de
#   necesitar transductores ultrasonicos especiales).
#
# DURACION_PULSO = 10 ms
#   Es un balance entre:
#     (a) tener energia suficiente en el pulso para buena SNR, y
#     (b) no generar una "zona ciega" demasiado grande: mientras se
#         esta transmitiendo, en general no se puede distinguir bien
#         un eco que llegue superpuesto a la propia transmision.
#   10 ms de pulso equivalen a una "zona ciega" de aprox.
#   d = v_s * t_pulso / 2 = 343 * 0.010 / 2 = 1.7 m si se usara deteccion
#   por energia simple. Sin embargo, gracias al chirp + correlacion
#   (ver mas abajo, "resolucion por compresion de pulso"), en la
#   practica se pueden resolver objetos MUCHO mas cercanos que eso.
#
# RESOLUCION EN DISTANCIA:
#   Un chirp con ancho de banda B = F1 - F0 = 6000 Hz tiene una
#   autocorrelacion con un pico angosto de ancho ~ 1/B. Esto se traduce
#   en una resolucion de distancia de:
#       delta_r = v_s / (2*B) = 343 / (2*6000) = 0.0286 m = 2.86 cm
#   Es decir, la correlacion puede distinguir dos objetos separados por
#   apenas ~3 cm, aunque el pulso dure 10 ms (bastante mas largo que
#   eso en distancia equivalente). Esto se comprueba en el
#   Experimento 2, donde 3 ecos que se ven totalmente superpuestos en
#   la senal cruda (Fig. 4, panel superior) se separan claramente en
#   la correlacion (Fig. 4, panel inferior).
#
# RETARDOS Y ATENUACIONES SIMULADAS
#   Los retardos de eco (p. ej. 6 ms, o 4/9/14 ms) se escogieron para
#   representar distancias fisicamente razonables de probar en un
#   escritorio/salon (d = v_s*tau/2): 6 ms -> ~1.03 m. Las atenuaciones
#   (0.3 a 0.8) son arbitrarias pero decrecientes con la distancia,
#   para representar de forma simplificada que un objeto mas lejano
#   refleja menos energia de vuelta al receptor.
#
# NIVELES DE RUIDO
#   Se barre en escala creciente (0.02, 0.6, 3.0, 15.0) para cubrir
#   desde un caso "limpio" hasta un caso donde el ruido supera por
#   mucho la amplitud del eco (~0.6 tras atenuacion). El objetivo es
#   encontrar el punto donde el detector deja de funcionar, para poder
#   discutir en el documento cual es el limite practico del metodo.
#
# VELOCIDAD DEL SONIDO v_s = 343 m/s
#   Valor estandar para aire a ~20 grados C a nivel del mar.
# --------------------------------------------------------------------
FS = 44100          # frecuencia de muestreo [Hz]
DURACION_PULSO = 0.01   # 10 ms de chirp transmitido
F0, F1 = 2000, 8000     # barrido de 2 kHz a 8 kHz
VEL_SONIDO = 343.0      # m/s (aprox. a nivel del mar, 20 C)


def mostrar_resultado_delay(nombre, retardo_real_muestras, n_directo, n_fft, fs):
    """Imprime en consola una comparacion legible entre el retardo real
    (conocido porque nosotros mismos lo simulamos) y el estimado."""
    print(f"--- {nombre} ---")
    print(f"  Retardo real       : {retardo_real_muestras} muestras "
          f"({retardo_real_muestras/fs*1000:.3f} ms)")
    print(f"  Estimado (directa) : {n_directo} muestras "
          f"({n_directo/fs*1000:.3f} ms)")
    print(f"  Estimado (FFT)     : {n_fft} muestras "
          f"({n_fft/fs*1000:.3f} ms)")
    d_directo = VEL_SONIDO * (n_directo/fs) / 2
    d_fft = VEL_SONIDO * (n_fft/fs) / 2
    print(f"  Distancia estimada directa : {d_directo*100:.2f} cm")
    print(f"  Distancia estimada FFT     : {d_fft*100:.2f} cm")
    print()


# ==========================================================================
# EXPERIMENTO 1: Caso base - un solo eco
# ==========================================================================
def experimento_1_caso_base():
    t_x, x = generar_chirp(FS, DURACION_PULSO, F0, F1)

    retardo_real_s = 0.006   # 6 ms -> objeto a ~1.03 m (ida y vuelta)
    t_y, y, retardos_m = generar_senal_recibida(
        x, FS,
        retardos_s=[retardo_real_s],
        atenuaciones=[0.6],
        ruido_std=0.05,
        duracion_total_s=0.02,
    )

    R_directa = correlacion_directa(y, x)
    R_fft = correlacion_fft(y, x)

    n_directo, _ = estimar_retardo(R_directa, FS)
    n_fft, _ = estimar_retardo(R_fft, FS)

    mostrar_resultado_delay("Experimento 1 (un eco)", retardos_m[0],
                             n_directo, n_fft, FS)

    # --- Figura: senal transmitida y recibida ---
    fig, axs = plt.subplots(2, 1, figsize=(9, 6), sharex=False)
    axs[0].plot(t_x * 1000, x, color="tab:blue")
    axs[0].set_title("Senal transmitida (chirp conocido)")
    axs[0].set_xlabel("Tiempo [ms]")
    axs[0].set_ylabel("Amplitud")
    axs[0].grid(True, alpha=0.3)

    axs[1].plot(t_y * 1000, y, color="tab:orange")
    axs[1].axvline(retardo_real_s * 1000, color="green", linestyle="--",
                    label=f"Retardo real = {retardo_real_s*1000:.1f} ms")
    axs[1].set_title("Senal recibida (transmision directa + eco + ruido)")
    axs[1].set_xlabel("Tiempo [ms]")
    axs[1].set_ylabel("Amplitud")
    axs[1].legend()
    axs[1].grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(f"{CARPETA_SALIDA}/1_senales_tx_rx.png", dpi=150)
    plt.close()

    # --- Figura: correlacion directa vs FFT ---
    t_R = np.arange(len(R_directa)) / FS * 1000  # ms

    fig, axs = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
    axs[0].plot(t_R, R_directa, color="tab:blue")
    axs[0].axvline(n_directo / FS * 1000, color="red", linestyle="--",
                    label=f"Pico detectado = {n_directo/FS*1000:.2f} ms")
    axs[0].set_title("Correlacion cruzada - Implementacion DIRECTA")
    axs[0].set_ylabel("R[m]")
    axs[0].legend()
    axs[0].grid(True, alpha=0.3)

    axs[1].plot(t_R, R_fft, color="tab:purple")
    axs[1].axvline(n_fft / FS * 1000, color="red", linestyle="--",
                    label=f"Pico detectado = {n_fft/FS*1000:.2f} ms")
    axs[1].set_title("Correlacion cruzada - Implementacion via FFT")
    axs[1].set_xlabel("Retardo [ms]")
    axs[1].set_ylabel("R[m]")
    axs[1].legend()
    axs[1].grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(f"{CARPETA_SALIDA}/2_correlacion_directa_vs_fft.png", dpi=150)
    plt.close()

    # --- Figura: diferencia numerica entre ambos metodos ---
    diferencia = R_directa - R_fft
    plt.figure(figsize=(9, 3))
    plt.plot(t_R, diferencia, color="black")
    plt.title("Diferencia numerica: Correlacion directa - Correlacion FFT")
    plt.xlabel("Retardo [ms]")
    plt.ylabel("Diferencia")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(f"{CARPETA_SALIDA}/3_diferencia_directa_fft.png", dpi=150)
    plt.close()

    print(f"Error maximo absoluto entre metodos: {np.max(np.abs(diferencia)):.2e}")
    print("(Deberia ser practicamente cero: ambos calculan lo mismo,")
    print(" solo cambia el algoritmo usado para llegar al resultado)\n")


# ==========================================================================
# EXPERIMENTO 2: Multiples ecos (varios objetos)
# ==========================================================================
def experimento_2_multiples_ecos():
    t_x, x = generar_chirp(FS, DURACION_PULSO, F0, F1)

    retardos_reales_s = [0.004, 0.009, 0.014]   # 3 "objetos" a distintas distancias
    atenuaciones = [0.8, 0.5, 0.3]              # mas lejos -> mas atenuado

    t_y, y, retardos_m = generar_senal_recibida(
        x, FS,
        retardos_s=retardos_reales_s,
        atenuaciones=atenuaciones,
        ruido_std=0.04,
        duracion_total_s=0.025,
    )

    R_fft = correlacion_fft(y, x)
    picos_m, picos_t = encontrar_picos(R_fft, FS, umbral_relativo=0.35,
                                        distancia_min_muestras=int(0.001 * FS))

    print("--- Experimento 2 (multiples ecos) ---")
    print(f"  Retardos reales   (ms): {[round(r*1000,2) for r in retardos_reales_s]}")
    print(f"  Retardos detectados (ms): {[round(t*1000,2) for t in picos_t]}")
    print()

    t_R = np.arange(len(R_fft)) / FS * 1000

    fig, axs = plt.subplots(2, 1, figsize=(9, 6))
    axs[0].plot(t_y * 1000, y, color="tab:orange")
    for r in retardos_reales_s:
        axs[0].axvline(r * 1000, color="green", linestyle="--", alpha=0.6)
    axs[0].set_title("Senal recibida con 3 objetos (ecos superpuestos + ruido)")
    axs[0].set_xlabel("Tiempo [ms]")
    axs[0].set_ylabel("Amplitud")
    axs[0].grid(True, alpha=0.3)

    axs[1].plot(t_R, R_fft, color="tab:purple")
    for p in picos_m:
        axs[1].axvline(p / FS * 1000, color="red", linestyle="--", alpha=0.8)
    axs[1].set_title("Correlacion (FFT): picos = objetos detectados")
    axs[1].set_xlabel("Retardo [ms]")
    axs[1].set_ylabel("R[m]")
    axs[1].grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(f"{CARPETA_SALIDA}/4_multiples_ecos.png", dpi=150)
    plt.close()


# ==========================================================================
# EXPERIMENTO 3: Robustez ante ruido
# ==========================================================================
def experimento_3_robustez_ruido():
    t_x, x = generar_chirp(FS, DURACION_PULSO, F0, F1)
    retardo_real_s = 0.006

    niveles_ruido = [0.02, 0.6, 3.0, 15.0]
    fig, axs = plt.subplots(len(niveles_ruido), 1, figsize=(9, 10), sharex=True)

    errores_ms = []
    for i, ruido_std in enumerate(niveles_ruido):
        t_y, y, retardos_m = generar_senal_recibida(
            x, FS, retardos_s=[retardo_real_s], atenuaciones=[0.6],
            ruido_std=ruido_std, duracion_total_s=0.02,
        )
        R_fft = correlacion_fft(y, x)
        n_fft, t_fft = estimar_retardo(R_fft, FS)
        error_ms = abs(t_fft - retardo_real_s) * 1000
        errores_ms.append(error_ms)

        t_R = np.arange(len(R_fft)) / FS * 1000
        axs[i].plot(t_R, R_fft, color="tab:purple")
        axs[i].axvline(retardo_real_s * 1000, color="green", linestyle="--",
                        label="Retardo real")
        axs[i].axvline(t_fft * 1000, color="red", linestyle=":",
                        label="Retardo detectado")
        axs[i].set_title(f"Ruido std = {ruido_std}  |  error = {error_ms:.3f} ms")
        axs[i].legend(fontsize=8)
        axs[i].grid(True, alpha=0.3)

    axs[-1].set_xlabel("Retardo [ms]")
    plt.tight_layout()
    plt.savefig(f"{CARPETA_SALIDA}/5_robustez_ruido.png", dpi=150)
    plt.close()

    print("--- Experimento 3 (robustez ante ruido) ---")
    for r, e in zip(niveles_ruido, errores_ms):
        print(f"  ruido_std = {r:>4} -> error de retardo = {e:.4f} ms")
    print()


# ==========================================================================
# EXPERIMENTO 4: Comparacion de tiempo de ejecucion (directa vs FFT)
# ==========================================================================
# ==========================================================================
# EXPERIMENTO 4: Comparacion de tiempo de ejecucion (directa vs FFT)
# ==========================================================================
def experimento_4_tiempos_ejecucion():
    tamanos_N = [500, 1000, 2000, 4000, 8000, 16000, 32000, 64000]
    M = 440  # largo fijo de la plantilla (senal conocida), ~10 ms a 44100 Hz
    REPETICIONES = 5  # se repite cada medicion y se toma el minimo, para
                       # reducir el ruido de medicion (interrupciones del SO, etc.)

    tiempos_directa = []
    tiempos_fft = []

    t_x, x = generar_chirp(FS, DURACION_PULSO, F0, F1)
    x = x[:M]

    for N in tamanos_N:
        y = np.random.normal(0, 0.1, N)
        y[:M] += x  # inserta la plantilla al inicio, no importa para medir tiempo

        muestras_directa = []
        muestras_fft = []
        for _ in range(REPETICIONES):
            inicio = time.perf_counter()
            correlacion_directa(y, x)
            muestras_directa.append(time.perf_counter() - inicio)

            inicio = time.perf_counter()
            correlacion_fft(y, x)
            muestras_fft.append(time.perf_counter() - inicio)

        tiempos_directa.append(min(muestras_directa))
        tiempos_fft.append(min(muestras_fft))

    print("--- Experimento 4 (tiempos de ejecucion) ---")
    print(f"{'N':>8} | {'Directa [s]':>12} | {'FFT [s]':>12} | {'Aceleracion':>12}")
    for N, td, tf in zip(tamanos_N, tiempos_directa, tiempos_fft):
        print(f"{N:>8} | {td:>12.6f} | {tf:>12.6f} | {td/tf:>11.1f}x")
    print()

    plt.figure(figsize=(8, 5))
    plt.plot(tamanos_N, tiempos_directa, "o-", label="Correlacion directa (O(N*M))")
    plt.plot(tamanos_N, tiempos_fft, "s-", label="Correlacion via FFT (O(N log N))")
    plt.yscale("log")
    plt.xlabel("Tamano de la senal recibida (N) [muestras]")
    plt.ylabel("Tiempo de ejecucion [s] (escala log)")
    plt.title("Comparacion de tiempo de ejecucion: correlacion directa vs FFT")
    plt.legend()
    plt.grid(True, which="both", alpha=0.3)
    plt.tight_layout()
    plt.savefig(f"{CARPETA_SALIDA}/6_comparacion_tiempos.png", dpi=150)
    plt.close()


if __name__ == "__main__":
    print("=" * 70)
    print(" EXPERIMENTOS DE DETECCION DE ECOS - CE1110")
    print("=" * 70)
    print()

    experimento_1_caso_base()
    experimento_2_multiples_ecos()
    experimento_3_robustez_ruido()
    experimento_4_tiempos_ejecucion()

    print("Listo. Todas las figuras se guardaron en la carpeta:", CARPETA_SALIDA)