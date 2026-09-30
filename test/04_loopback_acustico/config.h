#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define PIN_TX_DAC          25  // salida dac1 hacia el amplificador PAM8403
#define PIN_RX_ADC          34  // entrada adc1 conectada al microfono
#define PIN_LED_STATUS       2  // led azul integrado en la placa

#define FS_HZ               10000       // frecuencia de muestreo de 10 kHz (Ts = 100 us)
#define TS_US               (1000000 / FS_HZ)

#define FREQ_SONAR_HZ       1500.0f     // tono de prueba de 1.5 kHz
#define VEL_SONIDO_MS       343.0f

#define N_PULSO_TX          64          // 64 muestras de pulso (6.4 ms)
#define N_CAPTURA_RX        256         // 256 muestras de captura (25.6 ms)
#define N_FFT_PUNTOS        512         // zero-padding para la fft radix-2
#define N_CORRELACION       (N_CAPTURA_RX - N_PULSO_TX + 1)

#define PERIODO_TEST_MS     500         // disparo cada 500 ms
#define VELOCIDAD_SERIAL    115200

#endif // CONFIG_H
