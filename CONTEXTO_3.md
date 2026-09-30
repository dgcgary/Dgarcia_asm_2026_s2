# Bitacora de Cambios — Sesion de Diagnostico y Version Final (Contexto 3)

**Curso:** CE1110 — Analisis de Senales Mixtas  
**Institucion:** Tecnologico de Costa Rica (TEC)  
**Estudiantes:** David Garcia Cruz, Saddy Guzman Rojas  
**Fecha:** 29 de Setiembre, 2026  
**Rama Git:** `esp32-firmware`

---

## 1. Resumen Ejecutivo de la Sesion

En esta sesion se paso de la etapa de depuracion ciega a la **inspeccion visual y diagnostico exhaustivo en tiempo real** de las senales acusticas del radar (Test 6), se identificaron y resolvieron los problemas fisicos de multitrayectoria, y se consolido el **firmware definitivo para la operacion del sistema (Test 5)**.

---

## 2. Desarrollo del Test 6: Diagnostico Visual de Senales

### 2.1 Implementacion de Firmware y Script Python
* **Firmware (`06_diagnostico_radar.ino`):** 
  Se diseno un firmware especializado que transmite un paquete completo en formato JSON cada 2 segundos por el puerto serial (115200 baudios), conteniendo los vectores crudos de 256 muestras de la senal del microfono, la senal filtrada por el Biquad IIR, la referencia del Chirp y el vector completo de correlacion cruzada.
* **Interfaz Grafica (`test/diagnostico_radar.py`):**
  Script en Python usando `matplotlib` que grafica en tiempo real tres paneles sincronizados:
  1. **Panel 1 (Tx):** Chirp sintetizado (1000 Hz a 3000 Hz con ventana Hann).
  2. **Panel 2 (Rx):** Senal capturada por el microfono MAX4466 (cruda y filtrada con Biquad).
  3. **Panel 3 (DSP):** Curva de Correlacion Cruzada vs Distancia en centimetros, con la Zona Ciega sombreada en rosa y el pico detectado marcado con un asterisco rojo.

---

## 3. Hallazgos Experimentales y Diagnostico Fisico

### 3.1 Explicacion del Comportamiento a Corta Distancia (Prueba a 10 cm)
* **Sintoma observado:** Al colocar un bloque a 10 cm, el radar reportaba 39.7 cm.
* **Causa fisica diagnosticada:** 
  * Un objeto a 10 cm implica un tiempo de vuelo de ida y vuelta de solo $0.58\text{ ms}$ (unas 6 muestras).
  * El pulso emitido por el parlante dura $3.2\text{ ms}$ (32 muestras) mas la cola de resonancia mecanica del cono (*ring-down*).
  * Por diseno, la **Zona Ciega** esta fijada en 22 muestras ($\approx 38.0\text{ cm}$) para evitar que el radar se auto-detecte con el sonido directo del parlante.
  * Al estar el objeto a 10 cm dentro de la mascara de la zona ciega, el algoritmo ignoro el eco cercano y tomo el primer remanente que supero el umbral al salir de la mascara (39.7 cm).
* **Conclusion de rango:** El limite fisico inferior de deteccion del radar quedo validado a partir de **38 cm**.

### 3.2 Deteccion de Multitrayectoria (Pared Lateral a 74 cm)
* **Sintoma observado:** 
  * A 80 cm reales: el radar marcaba 79.4 cm (precision < 1%).
  * A 120 cm reales: el radar marcaba 122.5 cm, pero ocasionalmente saltaba a 74 cm.
* **Causa fisica:** El microfono MAX4466 posee un patron de captacion semiesferico amplio. La pared izquierda de la habitacion (a ~74 cm) generaba un rebote secundario constante. Por ley de atenuacion con la distancia, el eco cercano de la pared a 74 cm a veces competia en amplitud con el eco del objeto lejano a 120 cm.

---

## 4. Soluciones Implementadas: Hardware y Software

### 4.1 Solucion en Hardware: Bocina Conica Directiva en el Microfono
* Se construyo e instalo un **cono/embudo de papel/cartulina** frente al microfono MAX4466.
* **Efecto:** Otorgo directividad frontal al microfono, bloqueando las ondas sonoras que llegaban desde los lados (pared a 74 cm) y captando unicamente el eco frontal rebotado por el obstaculo objetivo.
* **Resultado:** Con solo este cono se elimino completamente la interferencia de la pared lateral.

### 4.2 Solucion en Software: Filtro de Mediana Movil (5 Muestras)
* Se programo la estructura `FiltroMediana` en `dsp_radar.h` y `dsp_radar.cpp`.
* Guarda las ultimas 5 lecturas consecutivas y calcula la mediana estadistica.
* Si en un disparo aislado entra un ruido espurio, la mediana lo descarta automaticamente, manteniendo la salida fija y estable.

---

## 5. Actualizacion y Consolidacion del Firmware Final (Test 5)

Se actualizo el firmware definitivo en `test/05_radar_acustico/` unificando todos los avances:

1. **Filtro Pasa Banda Biquad IIR:** Integrado en el pipeline DSP antes de la FFT para atenuar ruido fuera de la banda 1-3 kHz.
2. **Calculo Dinamico de $f_{s,\text{real}}$:** Mide con `micros()` la duracion real de cada rafaga de captura para corregir pequenas variaciones del ADC ($\approx 9930\text{ Hz}$).
3. **Filtro de Mediana Movil:** Activo en tiempo real sobre la distancia estimada.
4. **Frecuencia de Repeticion de Disparo:** Configurada a $250\text{ ms}$ ($4\text{ Hz}$), logrando una respuesta fluida al movimiento de objetos.
5. **Salida Serial Formateada:** Reporte con telemetria completa y barra grafica ASCII para visualizacion directa en el Monitor Serial del IDE.

---

## 6. Documentacion Creada

1. **`resumen.md`:** Documento maestro en la raiz del proyecto para estudio y defensa del curso CE1110. Contiene fundamentacion matematica, ecuaciones, esquemas de hardware, explicacion de codigo bloque por bloque y banco de preguntas de examen sin emoticones.
2. **`CONTEXTO_3.md`:** Esta bitacora que documenta todos los cambios, pruebas y decisiones de ingenieria tomadas en la sesion.
