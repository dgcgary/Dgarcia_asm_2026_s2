/**
 * ============================================================================
 * CE1110 - Análisis de Señales Mixtas | Instituto Tecnológico de Costa Rica
 * Proyecto: Radar Acústico Monostático
 * Archivo: config.h - Configuración para Test 04 (Loopback Acústico)
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define PIN_TX_DAC          25  // DAC1 integrado (Salida hacia PAM8403)
#define PIN_RX_ADC          34  // ADC1_CH6 (Entrada desde HW-484 AO)
#define PIN_LED_STATUS       2  // LED azul integrado

#define FS_HZ               10000       // Frecuencia de muestreo 10 kHz (Ts = 100 us)
#define TS_US               (1000000 / FS_HZ)

#define FREQ_SONAR_HZ       1500.0f     // Tono de 1.5 kHz (muy audible en parlantes pequeños)
#define VEL_SONIDO_MS       343.0f

// Pulso de prueba: 64 muestras = 6.4 ms (un 'bip' claro y fuerte)
#define N_PULSO_TX          64          
// Ventana de captura: 256 muestras = 25.6 ms
#define N_CAPTURA_RX        256         
// FFT Zero-Padding (Potencia de 2 >= 256 + 64 - 1 = 319)
#define N_FFT_PUNTOS        512         
// Puntos válidos de correlación (256 - 64 + 1 = 193)
#define N_CORRELACION       (N_CAPTURA_RX - N_PULSO_TX + 1)

#define PERIODO_TEST_MS     500   // Disparo cada 500 ms
#define VELOCIDAD_SERIAL    115200

#endif // CONFIG_H
