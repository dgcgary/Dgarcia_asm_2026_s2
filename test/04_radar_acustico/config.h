/**
 * ============================================================================
 * CE1110 - Análisis de Señales Mixtas | Instituto Tecnológico de Costa Rica
 * Proyecto: Radar Acústico Monostático
 * Archivo: config.h - Parámetros y Configuración del Sistema
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ----------------------------------------------------------------------------
// 1. ASIGNACIÓN DE PINES (ESP32 de 38 pines)
// ----------------------------------------------------------------------------
#define PIN_TX_DAC          25  // DAC1 integrado (Salida hacia PAM8403)
#define PIN_RX_ADC          34  // ADC1_CH6 (Entrada desde HW-484 AO)
#define PIN_LED_STATUS       2  // LED azul integrado

// ----------------------------------------------------------------------------
// 2. PARÁMETROS ACÚSTICOS Y TEMPORALES
// ----------------------------------------------------------------------------
#define FS_HZ               10000       // Frecuencia de muestreo (10 kHz -> Ts = 100 us)
#define TS_US               (1000000 / FS_HZ) // 100 microsegundos

#define FREQ_SONAR_HZ       2500.0f     // Tono transmitido a 2.5 kHz
#define VEL_SONIDO_MS       343.0f      // Velocidad del sonido en m/s a 20°C

// ----------------------------------------------------------------------------
// 3. DIMENSIONAMIENTO DE BÚFERES Y FFT
// ----------------------------------------------------------------------------
// Pulso transmitido: 3.2 ms -> 32 muestras a 10 kHz
#define N_PULSO_TX          32          

// Búfer de captura recibida: 25.6 ms -> 256 muestras a 10 kHz (~4.3 metros máx)
#define N_CAPTURA_RX        256         

// Tamaño para FFT con Zero-Padding (Potencia de 2 >= N_RX + N_TX - 1 = 287)
#define N_FFT_PUNTOS        512         

// Puntos válidos de correlación lineal (256 - 32 + 1 = 225)
#define N_CORRELACION       (N_CAPTURA_RX - N_PULSO_TX + 1)

// ----------------------------------------------------------------------------
// 4. ZONA CIEGA Y UMBRALES DE DETECCIÓN
// ----------------------------------------------------------------------------
// Ignora los primeros ~20 cm directos (acople acústico directo emisor-receptor)
#define ZONA_CIEGA_MUESTRAS 12    
#define UMBRAL_CORR_MINIMO  120.0f // Amplitud mínima de correlación para considerar eco válido

// ----------------------------------------------------------------------------
// 5. CONFIGURACIÓN DE OPERACIÓN
// ----------------------------------------------------------------------------
#define PERIODO_DISPARO_MS  250   // 4 disparos por segundo
#define VELOCIDAD_SERIAL    115200

#endif // CONFIG_H
