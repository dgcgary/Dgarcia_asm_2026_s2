# Contexto y Estado del Proyecto: Radar Acústico Monostático (ESP32)
**Curso:** CE1110 — Análisis de Señales Mixtas | **Institución:** Tecnológico de Costa Rica (TEC)  
**Microcontrolador:** ESP32-WROOM-32D (38 pines) | **Entorno:** PlatformIO / C++ (.ino) en Linux (Arch Linux)

---

## 1. Resumen Ejecutivo y Objetivo del Sistema

El objetivo del proyecto es diseñar e implementar un **radar acústico monostático** en un microcontrolador ESP32 sin utilizar librerías de alto nivel ni sensores comerciales (como el HC-SR04).

### Principio de Funcionamiento:
```
 +--------------------------------------------------------------------------------+
 | 1. TRANSMISIÓN (DAC GPIO 25)                                                   |
 |    Sintetiza una ráfaga senoidal de 1.5 kHz modulada con ventana Hann.        |
 |    Amplificada por el PAM8403 y emitida por un parlante de 8Ω / 3W.            |
 +---------------------------------------+----------------------------------------+
                                         | (Onda acústica en el aire a vs = 343 m/s)
                                         v
 +--------------------------------------------------------------------------------+
 | 2. RECEPCIÓN (ADC GPIO 34)                                                     |
 |    El micrófono HW-484 captura simultáneamente el audio a fs = 10 kHz.        |
 |    Se remueve el offset DC de polarización (~1.34 V).                          |
 +---------------------------------------+----------------------------------------+
                                         |
                                         v
 +--------------------------------------------------------------------------------+
 | 3. PROCESAMIENTO DIGITAL DE SEÑALES (DSP Embebido)                             |
 |    - FFT Radix-2 Cooley-Tukey (512 puntos con Zero-Padding).                  |
 |    - Multiplicación espectral: R[k] = FFT(Rx) * conj(FFT(Tx)).                 |
 |    - IFFT Radix-2 para obtener la correlación cruzada lineal en el tiempo.     |
 +---------------------------------------+----------------------------------------+
                                         |
                                         v
 +--------------------------------------------------------------------------------+
 | 4. ESTIMACIÓN DE TIEMPO DE VUELO Y DISTANCIA                                   |
 |    - Se suprime la zona ciega inicial (acople directo emisor-receptor).        |
 |    - Se identifica el índice pico m_pico del eco.                              |
 |    - ToF: tau = m_pico / fs                                                    |
 |    - Distancia: d = (vs * tau) / 2                                             |
 |    - Reporte formateado en tiempo real por el puerto Serial (115200 baudios).   |
 +--------------------------------------------------------------------------------+
```

---

## 2. Diagrama Eléctrico y Conexiones de Hardware

```
   +------------------+                    +-----------------+
   |      ESP32       |                    |  PAM8403 (Amp)  |
   |                  |                    |                 |
   |          GPIO 25 +----[ + 10uF - ]--->| L (Audio In)    |
   |              GND +------------------->| G (Audio GND)   |
   |          V5 (5V) +------------------->| + (Power 5V)    |
   |              GND +------------------->| - (Power GND)   |
   |                  |                    | LOUT+ / LOUT- +---+
   |                  |                    +---------------+   |
   |                  |                                        | Parlante
   |                  |                    +-----------------+ | (8Ω / 3W)
   |                  |                    | HW-484 (Micro)  | |
   |              3V3 +------------------->| VCC / +         | |
   |              GND +------------------->| GND / G         +-+
   |          GPIO 34 |<-------------------| AO (Analog Out) |
   |                  |                    | DO (Sin Conectar)
   +------------------+                    +-----------------+
```

### Puntos Críticos del Hardware ya Verificados:
1. **Capacitor $C_1$ ($10\,\mu\text{F}$ electrolítico):** Polo positivo $(+)$ a `GPIO 25`, polo negativo $(-)$ al pin `L` del PAM8403. Bloquea el nivel DC de 1.65 V y solo deja pasar la onda AC.
2. **Potenciómetro del PAM8403:** Tiene interruptor de encendido integrado. Debe girarse en sentido horario hasta sentir el "clic" para encender y dar volumen.
3. **Parlante en `LOUT+` y `LOUT-`:** Es conexión diferencial en puente (BTL). **`LOUT-` NUNCA se conecta a tierra (GND).**
4. **Micrófono HW-484:** Alimentado exclusivamente a **`3.3V`** (`3V3`). El pin `DO` queda desconectado.
5. **Calibración del potenciómetro azul del HW-484:** Se calibró el tornillo multivuelta para fijar el voltaje de reposo en **$\approx 1.34\text{ V}$** (punto medio del ADC de 3.3V para máximo rango dinámico).
6. **Comportamiento electrónico:** El micrófono actúa como un amplificador inversor (los impactos de presión sonora provocan **picos de voltaje hacia abajo**, lo cual es físicamente normal).

---

## 3. Estructura del Repositorio y Módulos de Prueba

El código fue desarrollado de forma gradual y modular dentro de la carpeta `test/`:

```
Dgarcia_asm_2026_s2/
├── platformio.ini              # Archivo de configuración para compilar y flashear
├── .gitignore                  # Configurado para ignorar .pio/ y binarios
├── CONTEXTO_PROYECTO_RADAR.md  # Este documento de arquitectura
├── test/
│   ├── 01_blink_led/           # Test 1: Validación básica de placa ESP32 y USB
│   ├── 02_test_dac/            # Test 2: Generador de audio continuo (1 kHz) en DAC
│   ├── 03_test_adc/            # Test 3: Lectura analógica de micrófono y monitor Vpp
│   ├── 04_loopback_acustico/   # Test 4: Detección del pulso propio (acople directo)
│   └── 05_radar_acustico/      # Test 5: Firmware de Radar Acústico Monostático Completo
```

---

## 4. Resultados de los Tests Graduales (1 al 5)

### Test 01: Blink LED (`01_blink_led.ino`)
* **Objetivo:** Validar la cadena de compilación con PlatformIO, el puerto `/dev/ttyUSB0` y el chip CP210x en Arch Linux.
* **Resultado:** Exitoso. Parpadeo del LED azul en `GPIO 2`.

### Test 02: Generación DAC (`02_test_dac.ino`)
* **Objetivo:** Comprobar la síntesis de audio por hardware usando `driver/dac.h` y el amplificador PAM8403.
* **Resultado:** Exitoso. Se escucha un tono claro de $1000\text{ Hz}$ en el parlante.

### Test 03: Lectura ADC y Calibración (`03_test_adc.ino`)
* **Objetivo:** Leer `GPIO 34` a $10\text{ kHz}$ y calibrar el offset DC del micrófono.
* **Resultado:** Exitoso. Se ajustó el potenciómetro azul del HW-484 hasta obtener un voltaje estable de $1.34\text{ V}$ en reposo y respuesta dinámica ante sonidos.

### Test 04: Loopback Acústico Directo (`04_loopback_acustico.ino`)
* **Objetivo:** Validar que el ESP32 emite una ráfaga corta ($6.4\text{ ms}$ a $1.5\text{ kHz}$) y el micrófono la captura simultáneamente, procesando la señal mediante la FFT Radix-2 en C++.
* **Resultado:** **Éxito total.** 
  * Pico de correlación detectado en $m = 2 \dots 3$ muestras ($0.20 - 0.30\text{ ms}$), correspondiente a los $\approx 10\text{ cm}$ de separación física entre el parlante y el micrófono.
  * Amplitud de correlación alta ($330 - 550$).
  * Tiempo de cómputo DSP de la FFT/IFFT: **$3.4\text{ ms}$**.

---

## 5. Diagnóstico del Test 05: Radar Acústico Completo

### Observación Experimental Reportada:
Al ejecutar el **Test 05 (`05_radar_acustico.ino`)**, el sistema compila, emite los pulsos y corre el DSP correctamente, pero reporta lecturas de:
* Apuntando al techo (a $2.0\text{ m}$): reporta $\approx 25.0\text{ cm}$.
* Apuntando a la pared (a $40\text{ cm}$): reporta $\approx 22.0\text{ cm}$.

---

### Análisis Técnico de la Causa Raíz:

1. **Unidades de salida:** La terminal imprime en **centímetros (`cm`)**, no en metros. Es decir, $22.0\text{ cm}$ y $25.0\text{ cm}$.
2. **Reverberación del acople directo vs Zona Ciega:**
   * En `config.h`, la zona ciega actual está configurada en:
     $$\text{ZONA\_CIEGA} = 12\text{ muestras} \implies d_{\text{min}} = \frac{343\text{ m/s} \times (12 / 10000\text{ s})}{2} \times 100 = 20.58\text{ cm}$$
   * El pulso emitido por el parlante dura $6.4\text{ ms}$ ($64$ muestras). La resonancia electromecánica del cono del parlante y la reverberación directa en la mesa siguen vibrando hasta las muestras $m = 13 \dots 15$.
   * Como el algoritmo busca el valor máximo después de la muestra 12, **la cola residual del pulso directo (en $m = 13 \dots 15$) es mucho más fuerte que el eco rebotado en el techo o pared**, haciendo que el radar siempre detecte $22 - 25\text{ cm}$.

---

## 6. Plan de Acción y Mejoras para Calibrar el Test 05

Para que el radar detecte correctamente la distancia real al objeto externo:

1. **Ajustar la Zona Ciega en `config.h`:**
   * Aumentar `ZONA_CIEGA_MUESTRAS` de 12 a **`18` o `20` muestras** ($\approx 30 - 35\text{ cm}$) para enmascarar completamente el final de la vibración del parlante.
2. **Acortar el Pulso Transmitido ($N_{\text{tx}}$):**
   * Reducir $N_{\text{PULSO\_TX}}$ de 64 muestras ($6.4\text{ ms}$) a **`32` o `40` muestras** ($3.2 - 4.0\text{ ms}$). Al ser más corto, la cola de reverberación desaparece mucho más rápido.
3. **Aislamiento Físico Parlante-Micrófono:**
   * Colocar un pequeño trozo de cartón o espuma entre el parlante y el micrófono para evitar que el sonido viaje directo por los lados y forzar a que el micrófono solo reciba la onda frontal rebotada.
4. **Umbral de Detección Dinámico:**
   * Ajustar `UMBRAL_CORR_MINIMO` en `config.h` para descartar ruido de fondo cuando no hay obstáculos al frente.

---

## 7. Instrucciones Rápidas de Uso para el Equipo

### Para compilar y subir cualquier test con PlatformIO:

1. Abrir `platformio.ini` y verificar qué carpeta está seleccionada en `src_dir`:
   * Ejemplo para Test 05: `src_dir = test/05_radar_acustico`
2. Cerrar cualquier monitor serial abierto (`Ctrl + C`).
3. En la terminal de Linux ejecutar:
   ```bash
   pio run -t upload          # Compilar y subir al ESP32 por USB
   pio device monitor         # Abrir el monitor serial a 115200 baudios
   ```
   *(O usar los botones `✓`, `→` y `🔌` en la barra inferior de VS Code).*
