# Resumen General y Guia de Estudio: Radar Acustico Monostatico (ESP32)

**Curso:** CE1110 — Analisis de Senales Mixtas  
**Institucion:** Tecnologico de Costa Rica (TEC)  
**Estudiantes:** David Garcia Cruz, Saddy Guzman Rojas  
**Microcontrolador:** ESP32-WROOM-32D (Arquitectura Xtensa dual-core de 32 bits, 240 MHz)

---

## Indice de Contenidos

1. [Que es el Proyecto y Como Funciona](#1-que-es-el-proyecto-y-como-funciona)
2. [Fundamento Teorico y Formulas](#2-fundamento-teorico-y-formulas)
3. [Conexiones de Hardware y Electronica](#3-conexiones-de-hardware-y-electronica)
4. [Bitacora Paso a Paso: De Test 1 a Test 6](#4-bitacora-paso-a-paso-de-test-1-a-test-6)
5. [Explicacion del Codigo Fuente](#5-explicacion-del-codigo-fuente)
6. [Preguntas y Respuestas Clave para la Defensa](#6-preguntas-y-respuestas-clave-para-la-defensa)

---

## 1. Que es el Proyecto y Como Funciona

El objetivo del proyecto es construir un **radar acustico monostatico** usando un microcontrolador ESP32. 

A diferencia de un sensor comercial (como el HC-SR04, que entrega un resultado ya procesado), en este proyecto **todo el procesamiento de la senal se programa y ejecuta directamente en el ESP32**.

### Flujo general del sistema:
1. **Transmision (Tx):** El ESP32 genera por su DAC interno (GPIO 25) un pulso de sonido llamado **Chirp** (un barrido continuo de 1000 Hz a 3000 Hz que dura 3.2 ms).
2. **Emision:** La senal pasa por un capacitor de desacoplo, se amplifica con el integrado PAM8403 y suena en un parlante de 8 ohm / 3W.
3. **Propagacion:** El sonido viaja por el aire a 343 m/s, choca contra un obstaculo y rebota.
4. **Recepcion (Rx):** Un microfono MAX4466 capta el sonido y lo entrega al ADC del ESP32 (GPIO 34), que toma 256 muestras a una tasa de 10 kHz.
5. **Filtrado:** Se remueve el voltaje continuo (Offset DC) y se pasa la senal por un filtro digital pasa banda (1000 Hz a 3000 Hz) para eliminar ruidos externos.
6. **Correlacion Cruzada (DSP):** Se compara la senal recibida con el Chirp original usando la Transformada Rapida de Fourier (FFT Radix-2). Donde la correlacion tenga su valor maximo, ahi esta el eco.
7. **Calculo de Distancia:** Se descartan las primeras muestras (Zona Ciega), se mide el tiempo de vuelo y se calcula la distancia exacta en centimetros.
8. **Filtro de Mediana:** Se aplica un filtro de mediana de 5 muestras para estabilizar la lectura y descartar rebotes secundarios.

---

## 2. Fundamento Teorico y Formulas

### 2.1 Ecuacion del Radar Monostatico
En un radar monostatico el emisor y el receptor estan en el mismo lugar. Por lo tanto, el sonido recorre la distancia dos veces (ida y vuelta):

$$d = \frac{v_s \cdot \tau}{2}$$

Donde:
* $d$: Distancia al objeto (en metros o centimetros).
* $v_s$: Velocidad del sonido en el aire ($343\text{ m/s}$ a $20^\circ\text{C}$).
* $\tau$: Tiempo de vuelo o ToF (*Time of Flight*) en segundos.

Si el eco llega en la muestra numero $m$ a una frecuencia de muestreo $f_s$:
$$\tau = \frac{m}{f_s} \implies d = \frac{v_s \cdot m}{2 f_s}$$

* **Resolucion espacial por muestra:** Con $f_s = 10000\text{ Hz}$:
  $$\Delta d = \frac{343}{2 \times 10000} = 0.01715\text{ m} = 1.715\text{ cm por muestra}$$

---

### 2.2 Por que un Chirp y no un tono simple de frecuencia fija?
* **Problema del tono simple (ej. 1500 Hz fijo):** Su autocorrelacion es una senal periodica con muchos picos secundarios casi del mismo tamano. Con senales debiles o ruido, el algoritmo confunde los picos y reporta distancias incorrectas.
* **Ventaja del Chirp (1000 Hz a 3000 Hz):** Es un pulso con modulacion de frecuencia. Su autocorrelacion concentra toda la energia en un unico pico muy estrecho en el centro y cancela casi por completo los lados. Esto se conoce en ingenieria de radar como **compresion de pulso**.
* **Ventana de Hann:** Se multiplica el Chirp por una curva suave (Hann) al inicio y al final para que el sonido no empiece ni termine en seco, evitando ruidos de conmutacion (*clics* o *pops*).

---

### 2.3 Correlacion Cruzada mediante FFT
La correlacion cruzada mide que tanto se parece la senal grabada por el microfono con la senal transmitida a lo largo del tiempo.

En vez de hacer miles de multiplicaciones en el tiempo ($O(N^2)$), se usa el **Teorema de la Correlacion** en el dominio de la frecuencia:

$$R_{xs}[m] = \text{IFFT} \Big( \text{FFT}(x_{\text{rx}}) \cdot \text{conj}(\text{FFT}(s_{\text{tx}})) \Big)$$

* Se toma la FFT de la senal del microfono.
* Se toma la FFT de la senal patron transmitida y se le saca el complejo conjugado.
* Se multiplican punto a punto.
* Se aplica la Transformada Inversa (IFFT) para volver al tiempo.

**Por que se usan 512 puntos en la FFT (Zero-Padding)?**
Para que la correlacion sea lineal y no circular, el tamano $N_{\text{fft}}$ debe ser mayor o igual a $N_{\text{rx}} + N_{\text{tx}} - 1 = 256 + 32 - 1 = 287$. El algoritmo Radix-2 requiere una potencia de 2, por lo que se eligen **512 puntos rellenando el resto con ceros (Zero-Padding)**.

---

### 2.4 Filtro Digital Biquad Pasa Banda
Es un filtro digital IIR de 2do orden disenado para dejar pasar unicamente las frecuencias entre 1000 Hz y 3000 Hz:

$$y[n] = b_0 x[n] + b_1 x[n-1] + b_2 x[n-2] - a_1 y[n-1] - a_2 y[n-2]$$

Elimina el ruido ambiente de baja frecuencia (zumbido de 60 Hz de la red electrica, viento) y el ruido de alta frecuencia antes de la correlacion.

---

### 2.5 Que es la Zona Ciega y por que existe?
Como el parlante y el microfono estan pegados:
1. Cuando el parlante suena (3.2 ms), el microfono escucha el sonido directo de inmediato.
2. El cono del parlante sigue vibrando por inercia mecanica unos milisegundos despues de apagarse (*ring-down*).

Para evitar que el radar detecte el sonido directo del parlante como si fuera un objeto:
* Se define una **Zona Ciega de 22 muestras** ($\approx 38\text{ cm}$).
* El algoritmo ignora todo lo que ocurra antes de la muestra 22.
* **Rango util del radar:** de $38\text{ cm}$ a $250\text{ cm}$.

---

## 3. Conexiones de Hardware y Electronica

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

### Justificacion de componentes:
* **Capacitor de 10 uF en serie con GPIO 25:** El DAC del ESP32 entrega una senal con un voltaje DC de 1.65 V. El capacitor bloquea la corriente continua y solo deja pasar el sonido (AC), protegiendo el amplificador y el parlante.
* **Conexion BTL del parlante en PAM8403:** El parlante va conectado a `LOUT+` y `LOUT-`. `LOUT-` **nunca se conecta a GND** porque es una salida diferencial en puente.
* **Microfono MAX4466 alimentado a 3.3V:** Se alimenta a 3.3V para que su senal de salida no exceda el rango seguro del convertidor analogico-digital del ESP32.
* **Bocina conica de papel/cartulina en el microfono:** Un pequeno cono en forma de embudo colocado frente al microfono le otorga un patron de captacion directivo hacia el frente, evitando que el microfono escuche los ecos reflejados de las paredes laterales.

---

## 4. Bitacora Paso a Paso: De Test 1 a Test 6

* **Test 1 (`01_blink_led`):** Comprobacion del ESP32, conexion USB y entorno PlatformIO en Linux.
* **Test 2 (`02_test_dac`):** Generacion de un tono continuo de 1000 Hz por DAC para verificar el amplificador PAM8403 y el parlante.
* **Test 3 (`03_test_adc`):** Lectura del microfono por GPIO 34 a 10 kHz. Se verifico que el nivel de reposo estuviera centrado en ~1.5 V.
* **Test 4 (`04_loopback_acustico`):** Primera prueba de disparo y captura simultanea. El microfono detecto el pulso directo del parlante con un retardo de 0.2 ms y el DSP en C++ tardo solo 3.4 ms.
* **Test 5 (`05_radar_acustico`):** Primer firmware de radar completo. 
  * *Problema encontrado:* Marcaba siempre 22 cm sin importar donde estuviera el objeto.
  * *Causa:* La vibracion mecanica del parlante duraba mas que la zona ciega original (12 muestras).
  * *Solucion:* Se acorto el pulso de 64 a 32 muestras y se amplio la zona ciega a 22 muestras (38 cm).
* **Test 6 (`06_diagnostico_radar` + `diagnostico_radar.py`):** Sistema de diagnostico visual en Python con 3 graficas en tiempo real. 
  * *Descubrimiento:* Se detecto un eco competidor en 74 cm debido al rebote en la pared lateral izquierda.
  * *Solucion:* Se agrego el cono direccional al microfono y se implemento un filtro de mediana de 5 muestras en el firmware.

---

## 5. Explicacion del Codigo Fuente

### 5.1 Parametros en `config.h`
* `FS_HZ = 10000`: Frecuencia de muestreo (10 kHz, una muestra cada 100 us).
* `F0_SONAR_HZ = 1000.0f` y `F1_SONAR_HZ = 3000.0f`: Frecuencias inicial y final del Chirp.
* `N_PULSO_TX = 32`: 32 muestras de transmision (3.2 ms).
* `N_CAPTURA_RX = 256`: 256 muestras de grabacion (25.6 ms, equivalente a ~4.3 metros de alcance teorico).
* `N_FFT_PUNTOS = 512`: Tamano de la FFT con Zero-Padding.
* `ZONA_CIEGA_MUESTRAS = 22`: Mascara inicial de 38 cm.
* `UMBRAL_CORR_MINIMO = 500.0f`: Nivel minimo de energia para aceptar un eco.
* `PERIODO_DISPARO_MS = 250`: 4 mediciones por segundo.

### 5.2 Estructura del DSP en `dsp_radar.cpp`
* `dsp_sintetizar_pulso()`: Genera la tabla del Chirp y le aplica la ventana Hann multiplicando por $0.5 \cdot (1 - \cos(\dots))$.
* `dsp_fft_radix2()`: Implementacion propia del algoritmo de mariposas Cooley-Tukey (in-place) con reordenamiento por inversion de bits (*Bit-Reversal*).
* `dsp_correlacion_cruzada()`: Aplica Zero-Padding, calcula las FFTs, multiplica por el complejo conjugado y saca la IFFT.
* `BiquadBandpass`: Filtro IIR que procesa cada muestra de entrada eliminando ruidos fuera de la banda 1-3 kHz.
* `FiltroMediana`: Almacena las ultimas 5 distancias validas y selecciona el valor intermedio (mediana), eliminando cualquier medicion atipica o rebote aislado.

### 5.3 Bucle principal en `05_radar_acustico.ino`
1. Emite la ráfaga por el DAC y lee simultáneamente el ADC en un bucle sincronizado de 100 microsegundos.
2. Mide el tiempo total real con `micros()` para calcular `fs_real`.
3. Resta el promedio de las muestras para eliminar el nivel DC.
4. Pasa la senal por el filtro Biquad.
5. Ejecuta la correlacion FFT Radix-2.
6. Busca el valor maximo a partir de la muestra 22 (`ZONA_CIEGA_MUESTRAS`).
7. Si supera el umbral, calcula el ToF, la distancia y la pasa por el filtro de mediana.
8. Imprime los resultados en el Monitor Serial.

---

## 6. Preguntas y Respuestas Clave para la Defensa

### Pregunta 1: Por que no usaron un tono senoidal simple de 1 kHz y tuvieron que usar un Chirp?
**Respuesta:** Porque la autocorrelacion de un tono simple es periodica y tiene muchos picos secundarios casi de la misma altura que el pico real. Con ruido o senales debiles, el sistema confunde los picos y da lecturas falsas. El Chirp lineal (1 kHz a 3 kHz) realiza compresion de pulsos: concentra toda la energia en un unico pico central agudo y cancela los picos laterales, dando maxima precision.

### Pregunta 2: Por que el radar tiene una zona ciega de 38 cm?
**Respuesta:** Porque es un sistema monostatico donde el microfono esta al lado del parlante. Cuando el parlante suena, el microfono capta el sonido directo inmediatamente, y ademas el cono del parlante sigue vibrando unos milisegundos despues de apagarse (*ring-down*). Un objeto a 10 cm devolveria un eco en solo 0.58 ms, coincidiendo con el momento en que el parlante aun esta vibrando. La zona ciega de 22 muestras (~38 cm) enmascara este efecto para que el radar no se detecte a si mismo.

### Pregunta 3: Por que se necesitan 512 puntos en la FFT si solo se capturan 256 muestras?
**Respuesta:** Por el Teorema de la Correlacion Lineal. Multiplicar espectros en frecuencia produce una correlacion circular (con solapamiento periodico). Para obtener la correlacion lineal exacta se necesita que el tamano de la FFT sea al menos $N_{\text{rx}} + N_{\text{tx}} - 1 = 256 + 32 - 1 = 287$ puntos. Como el algoritmo Radix-2 exige una potencia de 2, se aplica Zero-Padding rellenando con ceros hasta 512 puntos.

### Pregunta 4: Por que calculan `fs_real` en cada disparo en lugar de usar 10000 Hz fijo?
**Respuesta:** La funcion `analogRead()` del ESP32 tarda un tiempo que puede variar levemente (alrededor de 10 microsegundos). Al medir con `micros()` la duracion exacta del bucle de captura de las 256 muestras, se obtiene la frecuencia de muestreo real ($f_{s,\text{real}} \approx 9930\text{ Hz}$), eliminando errores de escala en el calculo de la distancia.

### Pregunta 5: Que causaba la lectura falsa de 74 cm y como se soluciono?
**Respuesta:** Se debia a multitrayectoria acustica (*multipath*): el microfono captaba el rebote del sonido en la pared lateral izquierda ubicada a ~74 cm. Se soluciono en hardware colocando una bocina conica de cartulina en el microfono para otorgarle directividad frontal (rechazando reflexiones que vengan de los lados), y en software con un filtro de mediana de 5 muestras que descarta cualquier lectura atipica aislada.

### Pregunta 6: Para que sirve el capacitor de 10 uF entre el GPIO 25 y el PAM8403?
**Respuesta:** El DAC del ESP32 entrega una senal con un voltaje DC de 1.65 V. Si esa corriente continua entra al amplificador PAM8403, satura la entrada y calienta innecesariamente la bobina del parlante. El capacitor bloquea la componente continua (0 Hz) y deja pasar unicamente la senal de audio AC.

---

## 7. Valores y Metricas Resumen del Sistema

* **Frecuencia de muestreo:** $\approx 10\text{ kHz}$ ($100\,\mu\text{s}$ por muestra).
* **Senal transmitida:** Chirp lineal de $1000\text{ Hz}$ a $3000\text{ Hz}$ con ventana Hann (32 muestras = 3.2 ms).
* **Rango util medible:** $38\text{ cm}$ hasta $250\text{ cm}$.
* **Tiempo de computo DSP por pulso:** $\approx 3.5\text{ ms}$ (correlacion FFT Radix-2).
* **Tasa de actualizacion:** $4\text{ disparos por segundo}$ ($250\text{ ms}$).
* **Error de medicion experimental:** Menor al $2\%$ respecto a cinta metrica.
