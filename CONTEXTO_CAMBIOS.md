# Bitácora de Cambios — Proyecto Radar Acústico Monostático (ESP32)
**Curso:** CE1110 — Análisis de Señales Mixtas | **Institución:** Tecnológico de Costa Rica (TEC)
**Estudiantes:** David García Cruz, Saddy Guzmán Rojas

> Este documento resume, en orden cronológico, todos los cambios hechos al
> proyecto (software de simulación, firmware del ESP32, hardware y
> documentación) desde el `CONTEXTO_PROYECTO_RADAR.md` original. Sirve
> como referencia rápida de "qué se hizo y por qué" para retomar el
> trabajo o explicarlo en la defensa.

---

## 1. Parte software — Simulación en Python (Taller 2, Parte 3: Detección de Ecos)

### 1.1 Primera versión (correlación con `np.fft`)
Se construyeron `deteccion_ecos.py` y `experimentos_ecos.py`:
- `generar_chirp()`: genera un chirp lineal (barrido de frecuencia) con ventana Hanning.
- `generar_senal_recibida()`: simula la señal recibida (eco(s) retardado(s), atenuado(s), + ruido).
- `correlacion_directa()`: correlación cruzada por definición directa, O(N·M).
- `correlacion_fft()`: correlación cruzada vía teorema de convolución, usando inicialmente `np.fft` (biblioteca).
- `estimar_retardo()` / `encontrar_picos()`: ubican el/los eco(s) en la correlación.
- 4 experimentos: caso base (1 eco), múltiples ecos, robustez ante ruido, comparación de tiempos directa vs. FFT.

### 1.2 Migración a FFT propia (Cooley-Tukey)
El estudiante aportó su propia implementación de DFT y FFT (`dft()` y `fft()`, Radix-2 recursiva, hecha en Python puro para la Parte 2 del taller). Se integró así:
- Se creó **`experimentos_fft.py`**: contiene la implementación de `dft()`/`fft()` del estudiante, con el bloque de pruebas (comparación DFT vs FFT, gráficas de magnitud/fase) protegido bajo `if __name__ == "__main__":`, para que el archivo sea **importable sin efectos secundarios** desde otros scripts.
- Se modificó `correlacion_fft()` en `deteccion_ecos.py` para usar **esta FFT propia** (importada desde `experimentos_fft.py`) en vez de `np.fft`, agregando:
  - `siguiente_potencia_dos()`: la FFT Radix-2 exige N potencia de 2, así que se rellena con ceros hasta la siguiente potencia de 2.
  - `ifft_propia()`: no se programó una IFFT aparte; se reutiliza la misma FFT con la identidad `ifft(X) = conj(fft(conj(X))) / N`.
- **Hallazgo importante:** al medir tiempos de ejecución, la FFT propia (recursiva, Python puro) resultó **más lenta** que la correlación directa (vectorizada con NumPy), a pesar de tener menos operaciones (N log N vs. N·M). Se documentó como ejemplo real de la diferencia entre *complejidad algorítmica* y *rendimiento real de la implementación* (el costo de intérprete de Python por cada llamada recursiva pesa más que el ahorro en operaciones, para los tamaños de N probados).

### 1.3 Documentación generada
- `complemento_ecos.docx`: explicación del código (función por función, en lenguaje sencillo) — pensado como secciones f) y g) del documento del taller.
- `conclusiones_generales.docx`: una única conclusión en prosa (2 párrafos, sin viñetas) que cierra tanto la parte de FFT (Parte 2) como la de detección de ecos (Parte 3).
- Se dieron 5 referencias bibliográficas en formato IEEE (Oppenheim & Schafer, Cooley & Tukey, Knapp & Carter, NumPy, Matplotlib), con indicación de dónde citar cada una en el texto.

---

## 2. Parte hardware — Selección y cableado de componentes

### 2.1 Micrófono: de HW-484 a MAX4466
- Se empezó con un módulo **HW-484 V0.2** (tipo KY-038: electret + comparador LM393, 4 pines: `+`, `G`, `A0`, `D0`). Requería calibrar un potenciómetro para fijar el offset DC de reposo (~1.34V).
- Se migró a un módulo **MAX4466** (solo 3 pines: `VCC`, `GND`, `OUT`, sin salida digital). Diferencias clave documentadas:
  - El bias de reposo es fijo por diseño en ~VCC/2 (≈1.65V con 3.3V), no ajustable por trimmer (el trimmer del MAX4466 solo controla ganancia).
  - Es un amplificador **no inversor** (picos de presión sonora hacia arriba), a diferencia del HW-484 que invertía la señal.

### 2.2 Conexionado final (ESP32 DevKitC de 38 pines)
| Componente | Pin del ESP32 | Nota |
|---|---|---|
| PAM8403 (amplificador) — VCC | `5V` | |
| PAM8403 — GND | `GND` | |
| PAM8403 — L_IN (señal) | `GPIO 25` (DAC1) | vía capacitor de acople 10µF (bloquea DC, deja pasar solo AC) |
| Parlante 8Ω/3W | `LOUT+` / `LOUT-` del PAM8403 | conexión en puente (BTL); `LOUT-` **nunca** a GND |
| MAX4466 — VCC | `3V3` | no usar 5V, para no exceder el rango del ADC |
| MAX4466 — GND | `GND` | |
| MAX4466 — OUT | `GPIO 34` (ADC1, input-only) | ADC1 evita conflicto con WiFi (a diferencia de ADC2) |

### 2.3 Recomendaciones de aislamiento físico
- Colocar espuma/cartón entre parlante y micrófono para reducir acople acústico directo.
- (Pendiente de aplicar) Cono/embudo direccional de cartulina alrededor de parlante y micrófono, para reducir la captación de reflectores no deseados fuera del eje de interés.

---

## 3. Herramienta de diagnóstico — `osciloscopio.py`

Script en Python (pyserial + matplotlib) para visualizar en vivo la señal del micrófono leída por el puerto serial del ESP32. Problemas encontrados y solucionados, en orden:

1. **Puerto incorrecto:** el script traía `/dev/ttyUSB0` (ruta de Linux); en Windows los puertos son `COMx`. Se identificó el puerto correcto vía Administrador de Dispositivos.
2. **Driver faltante:** Windows detectaba el chip CP2102 pero sin driver instalado (aparecía en "Otros dispositivos" con advertencia, sin número de COM asignado). Se instaló el driver oficial de Silicon Labs (CP210x VCP).
3. **Crash de matplotlib/Tkinter (`AttributeError: 'FigureCanvasBase' object has no attribute 'restore_region'`):** causado por `blit=True` en `FuncAnimation`, incompatible con el backend de Tkinter de la distribución de Python usada (Microsoft Store). Solucionado cambiando a `blit=False, cache_frame_data=False`.
4. **El problema persistió incluso con `blit=False`:** se reemplazó `FuncAnimation` por un **loop manual** (`while` + `plt.pause()`), más simple y confiable en Windows, agregando contadores de "líneas recibidas" y "líneas parseadas" para diagnosticar en qué capa fallaba (conexión serial vs. parseo de texto vs. ventana gráfica).
5. **Bug de parseo:** la rama `elif "Vpp (Sonido):" in linea:` cortaba el texto buscando `"Offset DC:"` (que no está en esa línea), así que nunca lograba extraer el valor. Corregido para cortar en `"Vpp (Sonido):"`.
6. **Diagnóstico final:** el problema real era que **PlatformIO no tenía cargado el test correcto** en `platformio.ini` (`src_dir` apuntaba a otro test). Una vez corregido, el osciloscopio funcionó.
7. **Aclaración conceptual:** la gráfica muestra **Vpp** (voltaje pico a pico calculado sobre una ventana de muestras), no el voltaje instantáneo del micrófono — por eso en silencio se ve cerca de 0V (no cerca de 1.65V, que sí sería el bias real medido con multímetro). Se ajustó el título, etiquetas y límites del eje Y (`0.0 - 3.3V`) acorde a esto, y se quitó la línea de referencia de offset (ya no aplica a una gráfica de Vpp).

---

## 4. Firmware del ESP32 — `05_radar_acustico.ino` / `dsp_radar.cpp` / `config.h`

Problemas diagnosticados y corregidos, en el orden en que aparecieron durante las pruebas:

### 4.1 Falso eco por resonancia del parlante
**Síntoma:** el radar reportaba siempre ~22-25 cm sin importar la distancia real al objeto.
**Causa:** el pulso transmitido (64 muestras, 6.4 ms) dejaba una cola de vibración mecánica del parlante que se extendía más allá de la zona ciega original (12 muestras ≈ 20.6 cm), y esa cola era más fuerte que el eco real.
**Corrección en `config.h`:**
- `N_PULSO_TX`: 64 → 32 muestras (pulso más corto, la cola de resonancia muere más rápido).
- `ZONA_CIEGA_MUESTRAS`: 12 → 22 muestras (~37.7 cm), para tapar completamente la cola.
- `UMBRAL_CORR_MINIMO`: 100 → 500 (el ruido de fondo ronda 20-55, así que 500 da margen de sobra).

### 4.2 Frecuencia de muestreo real distinta a la nominal
**Síntoma:** las distancias reportadas tenían un sesgo sistemático (más chicas de lo esperado).
**Causa:** `analogRead()` del ESP32 tarda más de los 100 µs asumidos por muestra; la `fs` real medida resultó ~8000-9300 Hz en vez de los 10 000 Hz nominales, dependiendo de la corrida.
**Diagnóstico:** se instrumentó el `.ino` para medir el tiempo real del bucle de captura (`t_captura_total_us`), y se comparó contra el valor esperado (`N_CAPTURA_RX × TS_US`).
**Corrección en `05_radar_acustico.ino`:** se calcula `fs_real` en cada disparo a partir del tiempo de captura medido, y se usa esa `fs_real` (no la constante `FS_HZ`) al llamar `dsp_estimar_distancia()`. Esto hace que el sistema se autoajuste sin importar cuánto tarde realmente `analogRead()`.

### 4.3 Offset DC del micrófono
Ya estaba bien resuelto desde el inicio del firmware: en vez de restar una constante fija (que hubiera quedado obsoleta al cambiar de HW-484 a MAX4466), el `.ino` calcula el promedio real de cada captura (`media_dc`) y lo resta a todas las muestras — el sistema se adapta solo al bias real de cualquier micrófono usado.

### 4.4 Tono puro → Chirp
**Síntoma:** a mayor distancia (menor amplitud de eco), el sistema ocasionalmente "saltaba" a picos de correlación falsos.
**Causa:** el pulso transmitido era un tono puro fijo (1.5 kHz), cuya autocorrelación tiene lóbulos secundarios casi tan altos como el pico principal cuando la señal es débil.
**Corrección:**
- `dsp_sintetizar_pulso()` (en `dsp_radar.cpp`/`.h`) se modificó para generar un **chirp lineal** (barrido de frecuencia), igual que en la simulación de Python, en vez de un tono de frecuencia fija.
- `config.h`: se reemplazó `FREQ_SONAR_HZ` (un solo valor) por `F0_SONAR_HZ` / `F1_SONAR_HZ` (1000 Hz → 3000 Hz).
- `.ino`: actualizada la llamada a `dsp_sintetizar_pulso()` y el mensaje de banner de arranque para reflejar el nuevo esquema de barrido.

### 4.5 Reflectores múltiples (pendiente de resolver)
**Síntoma:** con el objeto de prueba fijo, el sistema alterna entre dos lecturas estables y de amplitud similar (ej. ~94 cm y ~40 cm), en vez de reportar una sola distancia consistente.
**Causa identificada:** el parlante/micrófono tienen un patrón de radiación relativamente ancho (no direccional como un láser); hay un segundo objeto/superficie real dentro de ese cono, a ~40 cm, reflejando con fuerza comparable al objeto de interés.
**Estado:** pendiente de confirmar físicamente qué objeto está a ~40 cm del sensor, y de aplicar alguna mitigación (cono/embudo acústico direccional, reubicación del sensor, material absorbente en el entorno, o un filtro de persistencia en software que exija que una lectura se repita en varios disparos consecutivos antes de reportarla como válida).

---

## 5. Resultados de validación (distancia real vs. medida)

| Distancia real (cinta métrica) | Distancia medida | Error absoluto | Observación |
|---|---|---|---|
| ~87.5 cm | 87.5 cm (13/14 lecturas estables) | ~0 cm | Antes de la corrección de `fs_real`; una vez corregida la escala, la misma distancia física dio 88.0 cm con la fs real medida ese día. |
| ~55 cm | 57.0 cm | 2.0 cm | |
| ~85 cm | 93.7-93.8 cm (lecturas estables) | ~8.8 cm | Coincide con reflector competidor a ~40 cm en el mismo set de pruebas — pendiente diagnosticar si el error viene del punto de referencia de medición o de interferencia entre ambos ecos. |

---

## 6. Documentación del proyecto (paper / informe)

- Se generó **`borrador_paper.md`**: esqueleto completo en Markdown para el artículo tipo paper exigido por el enunciado (máx. 4 páginas, formato IEEE/ACM), con las secciones Abstract, palabras clave, introducción, marco teórico, resultados de las Partes 2-5, conclusiones, recomendaciones y referencias — parcialmente redactado con base en todo el trabajo de esta bitácora, con una lista de pendientes explícita (completar tabla de precisión final, decidir solución a reflectores múltiples, traducir abstract al inglés, insertar figuras, verificar límite de 4 páginas).
- Se redactó el texto completo para las 3 secciones del documento de **Herramientas de Ingeniería** exigido por el enunciado (selección, aplicación y adaptación de técnicas/herramientas/métodos), incluyendo la mención explícita del uso de un asistente de IA como apoyo, tal como lo exige el enunciado del proyecto.

---

## 7. Pendientes generales del proyecto

- [ ] Resolver el problema de reflectores múltiples (Sección 4.5).
- [ ] Completar la tabla de precisión con al menos una tercera distancia de referencia (ej. ~150 cm).
- [ ] Aplicar aislamiento físico adicional (cono/embudo) entre parlante/micrófono y hacia el entorno.
- [ ] Considerar agregar un filtro de persistencia en software (exigir 2-3 lecturas consecutivas iguales antes de reportar un cambio de distancia).
- [ ] Migrar el contenido de `borrador_paper.md` a la plantilla LaTeX oficial (IEEE/ACM) provista por el profesor.
- [ ] Revisar y actualizar comentarios desactualizados en `config.h` (los que aún mencionan `N_PULSO_TX = 64` en el cálculo de `N_FFT_PUNTOS`/`N_CORRELACION`).
- [ ] Confirmar que el repositorio de Git cumple con la metodología de trabajo exigida (ramas `master`/`development`, commits incrementales, no un solo commit final).
