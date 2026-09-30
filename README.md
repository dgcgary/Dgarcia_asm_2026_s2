# Radar Acustico Monostatico (ESP32)

Sistema embebido de radar acustico para estimacion de distancia mediante compresion de pulsos (Chirp) y procesamiento digital de señales (DSP) en tiempo real, desarrollado sobre un microcontrolador ESP32-WROOM-32D.

**Curso:** CE1110 — Analisis de Señales Mixtas  
**Institucion:** Tecnologico de Costa Rica (TEC)  
**Estudiantes:** David Garcia Cruz, Saddy Guzman Rojas  

---

## Caracteristicas del Sistema

* **Sintesis de Pulso:** Generacion de ráfaga Chirp lineal de 1000 Hz a 3000 Hz modulada con ventana Hann por DAC (GPIO 25).
* **Adquisicion Analogica:** Captura de audio en tiempo real a 10 kHz (256 muestras) mediante microfono MAX4466 por ADC (GPIO 34).
* **Filtrado Digital:** Filtro pasa banda IIR Biquad de 2do orden (1000 Hz a 3000 Hz) para reduccion de ruido fuera de banda.
* **Correlacion Cruzada:** Algoritmo FFT/IFFT Radix-2 Cooley-Tukey con Zero-Padding a 512 puntos implementado en C++ (tiempo de ejecucion ~3.5 ms).
* **Estabilizacion de Distancia:** Filtro de mediana movil de 5 muestras para supresion de rebotes por multitrayectoria.
* **Rango de Medicion:** 38 cm a 250 cm con tasa de refresco de 4 Hz (250 ms).

---

## Conexion de Hardware

| Modulo / Componente | Pin ESP32 | Descripcion |
| :--- | :--- | :--- |
| **PAM8403 (Amp)** | `GPIO 25` (DAC1) | Entrada de audio L_IN a traves de capacitor electrolitico de 10 uF |
| **PAM8403 (Amp)** | `5V` (V5) / `GND` | Alimentacion principal del amplificador |
| **Parlante 8 ohm / 3W** | `LOUT+` / `LOUT-` | Salida diferencial en puente (BTL) del PAM8403 (no conectar a GND) |
| **MAX4466 (Micro)** | `3.3V` (3V3) / `GND` | Alimentacion regulada para bajo ruido |
| **MAX4466 (Micro)** | `GPIO 34` (ADC1_CH6) | Salida analogica OUT conectada al ADC |
| **LED de Estado** | `GPIO 2` | Indicador visual de disparo de pulso |

*Nota:* El microfono cuenta con un cono de cartulina directivo instalado al frente para atenuar reflexiones acusticas laterales.

---

## Estructura del Repositorio

```text
.
├── platformio.ini              # Configuracion de compilacion y carga
├── README.md                   # Descripcion general del repositorio
├── documentacion.md            # Documentacion tecnica detallada y guia de defensa
├── radar_serial/               # Codigo principal: radar autonomo para Monitor Serial
│   ├── radar_serial.ino
│   ├── config.h
│   ├── dsp_radar.h
│   └── dsp_radar.cpp
├── radar_graficas/             # Codigo principal: telemetria JSON para interfaz grafica
│   ├── radar_graficas.ino
│   ├── config.h
│   ├── dsp_radar.h
│   └── dsp_radar.cpp
├── test/
│   ├── 01_blink_led/           # Test 1: Verificacion de entorno y hardware basico
│   ├── 02_test_dac/            # Test 2: Comprobacion de salida DAC y amplificador
│   ├── 03_test_adc/            # Test 3: Calibracion de entrada ADC del microfono
│   ├── 04_loopback_acustico/   # Test 4: Prueba de emision y recepcion acustica directa
│   ├── 05_radar_acustico/      # Test 5: Prototipo previo de radar autonomo
│   ├── 06_diagnostico_radar/   # Test 6: Prototipo previo de telemetria
│   ├── diagnostico_radar.py    # GUI en Python con graficas de Tx, Rx y Correlacion
│   └── osciloscopio.py         # Visualizador basico de señal ADC
└── Taller2Avance/              # Scripts de simulacion previa del Taller 2
```

---

## Instrucciones de Uso

### 1. Compilar y Cargar el Radar Serial (Monitor Serial)

Por defecto, `platformio.ini` esta configurado para compilar `radar_serial`:

```bash
pio run -t upload
pio device monitor -b 115200
```

La terminal mostrara las lecturas de distancia en tiempo real con barra grafica:

```text
[ECO #0012] Distancia:  80.2 cm (Cruda:  79.8 cm) | ToF:  4.68 ms | Pico:  46 | Amp:   4210 | DSP: 6920 us | [======                   ]
```

### 2. Ejecutar el Radar con Diagnostico Grafico en Tiempo Real

1. En `platformio.ini`, configure `src_dir`:
   ```ini
   src_dir = radar_graficas
   ```
2. Cargue el firmware al ESP32:
   ```bash
   pio run -t upload
   ```
3. Ejecute la aplicacion grafica en Python:
   ```bash
   python3 test/diagnostico_radar.py
   ```

---

## Documentacion Completa

Para consultar la fundamentacion matematica, ecuaciones, analisis de diseño, bitacora cronologica y banco de preguntas para evaluacion, consulte:
* [documentacion.md](documentacion.md)
