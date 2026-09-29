#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ----------------------------------------------------------------------------
// 1. ASIGNACIÓN DE PINES
// ----------------------------------------------------------------------------
#define PIN_TX_DAC          25
#define PIN_RX_ADC          34
#define PIN_LED_STATUS       2

// ----------------------------------------------------------------------------
// 2. PARÁMETROS ACÚSTICOS Y TEMPORALES
// ----------------------------------------------------------------------------
#define FS_HZ               20000       // Frecuencia de muestreo (20 kHz)
#define TS_US               (1000000 / FS_HZ)

// Chirp lineal 1 kHz -> 6 kHz
// El ancho de banda útil es 1-6 kHz, y el filtro pasa banda se centra
// alrededor de 2.5-3.0 kHz para reducir ruido y mantener la señal útil.
#define F0_SONAR_HZ         1000.0f
#define F1_SONAR_HZ         6000.0f
#define VEL_SONIDO_MS       343.0f

// ----------------------------------------------------------------------------
// 3. DIMENSIONAMIENTO DE BÚFERES Y FFT
// ----------------------------------------------------------------------------
#define N_PULSO_TX          48
#define N_CAPTURA_RX        256
#define N_FFT_PUNTOS        512
#define N_CORRELACION       (N_CAPTURA_RX - N_PULSO_TX + 1)

// ----------------------------------------------------------------------------
// 4. ZONA CIEGA Y UMBRALES
// ----------------------------------------------------------------------------
#define ZONA_CIEGA_MUESTRAS 18
#define UMBRAL_CORR_MINIMO  700.0f
#define MAX_DISTANCIA_CM    250.0f

// Filtro de estabilidad para evitar saltos temporales
#define HISTORIAL_DISTANCIAS 7
#define TOLERANCIA_OUTLIER_CM 10.0f

// ----------------------------------------------------------------------------
// 5. CONFIGURACIÓN DE OPERACIÓN
// ----------------------------------------------------------------------------
#define PERIODO_DISPARO_MS  750
#define VELOCIDAD_SERIAL    115200

#endif