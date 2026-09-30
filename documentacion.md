# Documentacion Tecnica y Bitacora: Radar Acustico Monostatico (ESP32)

**Curso:** CE1110 — Analisis de Señales Mixtas  
**Institucion:** Tecnologico de Costa Rica (TEC)  
**Estudiantes:** David Garcia Cruz, Saddy Guzman Rojas  
**Microcontrolador:** ESP32-WROOM-32D (Xtensa dual-core de 32 bits, 240 MHz)

---

## Indice de Contenidos

1. [Vision General y Principio de Operacion](#1-vision-general-y-principio-de-operacion)
2. [Fundamentacion Matematica y DSP](#2-fundamentacion-matematica-y-dsp)
3. [Arquitectura de Hardware y Diagrama Electrico](#3-arquitectura-de-hardware-y-diagrama-electrico)
4. [Bitacora de Desarrollo y Pruebas (Test 1 al 6)](#4-bitacora-de-desarrollo-y-pruebas-test-1-al-6)
5. [Estructura del Codigo y Pipeline DSP](#5-estructura-del-codigo-y-pipeline-dsp)
6. [Guia de Respuestas para Defensa y Evaluacion](#6-guia-de-respuestas-para-defensa-y-evaluacion)

---

## 1. Vision General y Principio de Operacion

El proyecto consiste en el diseño e implementacion de un **radar acustico monostatico** sobre un microcontrolador ESP32. El sistema sintetiza, transmite, captura y procesa señales acusticas para medir la distancia a obstaculos en tiempo real sin utilizar sensores comerciales (como el HC-SR04) ni librerias externas de DSP.

### Flujo secuencial del sistema:
1. **Sintesis y Transmision (Tx):** El DAC del ESP32 (GPIO 25) genera un pulso Chirp lineal (1000 Hz a 3000 Hz) modulado con ventana Hann durante 3.2 ms (32 muestras a 10 kHz).
2. **Emision Acustica:** La señal pasa por un capacitor de desacoplo de 10 uF, se amplifica con el integrado PAM8403 y se emite mediante un parlante de 8 ohm / 3W.
3. **Propagacion:** La onda de presion viaja por el aire a la velocidad del sonido (vs = 343 m/s), impacta un obstaculo y se refleja de regreso.
4. **Recepcion (Rx):** Un microfono MAX4466 con cono directivo captura el eco acustico. El ADC del ESP32 (GPIO 34) adquiere 256 muestras a 10 kHz (~25.6 ms).
5. **Acondicionamiento y Filtrado:** Se elimina el nivel de voltaje continuo (Offset DC) y se procesa la señal con un filtro digital pasa banda IIR Biquad (1000 Hz a 3000 Hz).
6. **Procesamiento Espectral (DSP):** Se calcula la correlacion cruzada lineal entre la señal recibida y la referencia transmitida mediante FFT y IFFT Radix-2 Cooley-Tukey con Zero-Padding a 512 puntos.
7. **Estimacion de ToF y Distancia:** Se aplica una mascara de zona ciega (22 muestras = 38 cm) para omitir el acople directo, se detecta el pico de maxima energia y se calcula la distancia.
8. **Filtro de Mediana Movil:** Un buffer circular de 5 muestras estabiliza la medicion y descarta rebotes secundarios espurios (multitrayectoria).

---

## 2. Fundamentacion Matematica y DSP

### 2.1 Ecuacion de Distancia y Resolucion Espacial
En la configuracion monostatica el sonido realiza un trayecto doble (ida y vuelta):

$$d = \frac{v_s \cdot \tau}{2}$$

Donde:
* $d$: Distancia al obstaculo en metros.
* $v_s$: Velocidad del sonido ($343\text{ m/s}$ a $20^\circ\text{C}$).
* $\tau$: Tiempo de vuelo (Time-of-Flight) en segundos.

Relacionando el retardo con el indice de muestra $m$ y la frecuencia de muestreo $f_s$:

$$\tau = \frac{m}{f_s} \implies d = \frac{v_s \cdot m}{2 f_s}$$

* **Resolucion espacial:** Para $f_s = 10000\text{ Hz}$ y $v_s = 343\text{ m/s}$:
  $$\Delta d = \frac{343}{2 \times 10000} = 0.01715\text{ m} = 1.715\text{ cm por muestra}$$

---

### 2.2 Modulacion Chirp y Compresion de Pulso
En lugar de un tono senoidal simple de frecuencia fija (cuya autocorrelacion es periodica y genera multiples lobulos secundarios que confunden la deteccion), se utiliza un **Chirp lineal**:

* Barrido de frecuencia: $f_0 = 1000\text{ Hz}$ hasta $f_1 = 3000\text{ Hz}$.
* Tasa de barrido (Chirp Rate): $k = \frac{f_1 - f_0}{T_{\text{pulso}}}$.
* Fase instantanea: $\phi(t) = 2\pi \left( f_0 t + \frac{1}{2} k t^2 \right)$.
* Modulacion de amplitud: Ventana Hann para eliminar transitorios y ruido de conmutacion:
  $$w[n] = 0.5 \left( 1 - \cos\left(\frac{2\pi n}{N_{\text{tx}}-1}\right) \right)$$

La autocorrelacion del Chirp aproxima una funcion Sinc con un pico central agudo y lobulos laterales atenuados, maximizando la relacion señal a ruido (SNR).

---

### 2.3 Correlacion Cruzada Rapida mediante FFT
Para correlacionar la señal recibida $x[n]$ (tamaño $N_{\text{rx}} = 256$) con la plantilla $s[n]$ (tamaño $N_{\text{tx}} = 32$), se utiliza el teorema de correlacion en frecuencia:

$$R_{xs}[m] = \text{IFFT} \Big( \text{FFT}(x) \cdot \text{conj}(\text{FFT}(s)) \Big)$$

* **Requisito de Zero-Padding:** Para obtener correlacion lineal aperiodica sin solapamiento circular, se requiere $N_{\text{fft}} \ge N_{\text{rx}} + N_{\text{tx}} - 1 = 287$. Se selecciona **$N_{\text{fft}} = 512$** (potencia de 2 para algoritmo Radix-2).
* **Complejidad computacional:** Reduce el costo de $O(N_{\text{rx}} \cdot N_{\text{tx}})$ a $O(N_{\text{fft}} \log_2 N_{\text{fft}})$, ejecutandose en el ESP32 en $\approx 3.5\text{ ms}$.

---

### 2.4 Filtro Digital IIR Biquad
Filtro pasa banda digital de segundo orden en Direct Form I centrado en 2000 Hz con ancho de banda de 2000 Hz:

$$y[n] = b_0 x[n] + b_1 x[n-1] + b_2 x[n-2] - a_1 y[n-1] - a_2 y[n-2]$$

Atenua zumbidos de 60 Hz de la red electrica y ruidos fuera de la banda util antes de la correlacion.

---

### 2.5 Zona Ciega (Blind Zone)
Debido a la cercania fisica entre parlante y microfono:
* Durante los 3.2 ms de transmision ocurre acople acustico directo.
* El cono del parlante presenta inercia mecanica residual (ring-down).
* Se define una **zona ciega de 22 muestras** ($\approx 38.0\text{ cm}$).
* El algoritmo inicia la busqueda de picos a partir de la muestra 22, fijando el rango util del radar de **$38\text{ cm}$ a $250\text{ cm}$**.

---

## 3. Arquitectura de Hardware y Diagrama Electrico

```
       +------------------+                   +-----------------+
       |      ESP32       |                   |  PAM8403 (Amp)  |
       |                  |                   |                 |
       |  GPIO 25 (DAC1)  +---[ + 10uF - ]--->| L (Audio In)    |
       |  GND             +------------------>| G (Audio GND)   |
       |  5V (V5)         +------------------>| + (Power 5V)    |
       |  GND             +------------------>| - (Power GND)   |
       |                  |                   | LOUT+ / LOUT- +---+
       |                  |                   +---------------+   |
       |                  |                                       | Parlante
       |                  |                   +-----------------+ | (8 ohm / 3W)
       |                  |                   | MAX4466 (Micro) | |
       |  3.3V (3V3)      +------------------>| VCC             | |
       |  GND             +------------------>| GND             +-+
       |  GPIO 34 (ADC1)  |<------------------| OUT             |
       +------------------+                   +-----------------+
```

### Justificacion tecnica del conexionado:
1. **Capacitor de 10 uF en serie (GPIO 25 a PAM8403):** Bloquea el nivel DC de 1.65 V del DAC y permite el paso exclusivo de la componente AC de audio, previniendo saturacion del amplificador y calentamiento de la bobina.
2. **Salida BTL en PAM8403:** El parlante se conecta entre `LOUT+` y `LOUT-`. `LOUT-` no debe conectarse a tierra bajo ninguna circunstancia.
3. **Microfono MAX4466 a 3.3V:** Polarizado a 3.3V para acoplar su rango dinamico al convertidor ADC1 del ESP32 (GPIO 34).
4. **Bocina conica en microfono:** Embudo conico de cartulina de 4 a 6 cm de largo instalado frente al microfono para conferir directividad frontal y atenuar reflexiones acusticas laterales.

---

## 4. Bitacora de Desarrollo y Pruebas (Test 1 al 6)

### Test 1: Validacion de Plataforma (`test/01_blink_led/`)
* **Objetivo:** Comprobar la cadena de herramientas PlatformIO, puerto serie y funcionamiento del ESP32.
* **Resultado:** Exitoso. Parpadeo del LED integrado en GPIO 2.

### Test 2: Generacion DAC y Amplificacion (`test/02_test_dac/`)
* **Objetivo:** Validar la generacion analogica por hardware mediante `driver/dac.h` y el circuito PAM8403 + Parlante.
* **Resultado:** Exitoso. Emision continua de tono senoidal de 1 kHz.

### Test 3: Adquisicion ADC y Calibracion (`test/03_test_adc/`)
* **Objetivo:** Adquirir señal del microfono a 10 kHz y medir nivel de reposo DC.
* **Resultado:** Exitoso. Señal de reposo estable en $\approx 1850$ cuentas ADC ($\approx 1.5\text{ V}$).

### Test 4: Loopback Acustico Directo (`test/04_loopback_acustico/`)
* **Objetivo:** Probar transmision y captura sincronizada con calculo de correlacion cruzada FFT en C++.
* **Resultado:** Exitoso. Deteccion del acople directo en muestra $m = 2..3$ ($0.2\text{ ms}$). Tiempo de computo DSP de $3.4\text{ ms}$.

### Test 5: Radar Acustico Autonomo (`test/05_radar_acustico/`)
* **Problema observado:** El radar reportaba lecturas fijas de 22 cm a 25 cm independientemente de la distancia real.
* **Diagnostico:** La duracion del pulso original (64 muestras) y la resonancia mecanica del parlante excedian la zona ciega inicial (12 muestras).
* **Solucion:** Se redujo el pulso a 32 muestras y se amplio la zona ciega a 22 muestras (38 cm).

### Test 6: Diagnostico Visual en Tiempo Real (`test/06_diagnostico_radar/` + `test/diagnostico_radar.py`)
* **Implementacion:** Emision de telemetria en JSON desde el ESP32 e interfaz grafica en Python (Matplotlib) con tres paneles (Tx, Rx, Correlacion).
* **Hallazgo experimental:** A 120 cm el eco competia con un rebote lateral en la pared izquierda a 74 cm.
* **Solucion definitiva:** Instalacion de la bocina conica en el microfono y adicion del filtro de mediana movil de 5 muestras en el firmware.

---

## 5. Estructura del Codigo y Pipeline DSP

### 5.1 Parametros Globales (`config.h`)
* `FS_HZ = 10000`: Frecuencia de muestreo nominal.
* `F0_SONAR_HZ = 1000.0f`, `F1_SONAR_HZ = 3000.0f`: Rango de frecuencias del Chirp.
* `N_PULSO_TX = 32`: Duracion de emision (3.2 ms).
* `N_CAPTURA_RX = 256`: Ventana de escucha (25.6 ms).
* `N_FFT_PUNTOS = 512`: Puntos de la FFT con zero-padding.
* `ZONA_CIEGA_MUESTRAS = 22`: Mascara de acople inicial (~38 cm).
* `UMBRAL_CORR_MINIMO = 500.0f`: Umbral de deteccion.
* `PERIODO_DISPARO_MS = 250`: Tasa de repeticion de 4 disparos por segundo.

### 5.2 Modulos DSP (`dsp_radar.cpp` / `dsp_radar.h`)
* `dsp_sintetizar_pulso()`: Genera la tabla del Chirp lineal y aplica la ponderacion Hann.
* `dsp_fft_radix2()`: Implementacion in-place del algoritmo Cooley-Tukey con bit-reversal.
* `dsp_correlacion_cruzada()`: Ejecuta FFT directa, multiplicacion espectral conjugada e IFFT.
* `BiquadBandpass`: Estructura del filtro pasa banda IIR de segundo orden.
* `FiltroMediana`: Buffer circular de 5 elementos que calcula la mediana de las distancias.
* `dsp_estimar_distancia()`: Ubica el pico maximo despues de la zona ciega y calcula el ToF y la distancia metrica.

---
