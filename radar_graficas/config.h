#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Asignacion de pines en el ESP32
#define PIN_TX_DAC          25  // salida dac1 hacia el amplificador PAM8403
#define PIN_RX_ADC          34  // entrada adc1 conectada al microfono MAX4466
#define PIN_LED_STATUS       2  // led azul integrado en la placa

// Parametros acusticos del pulso
#define FS_HZ               10000       // frecuencia de muestreo nominal de 10 kHz (Ts = 100 us)
#define TS_US               (1000000 / FS_HZ)

#define F0_SONAR_HZ         1000.0f     // frecuencia inicial del barrido chirp
#define F1_SONAR_HZ         3000.0f     // frecuencia final del barrido chirp
#define VEL_SONIDO_MS       343.0f      // velocidad del sonido en el aire a 20 grados Celsius

// Dimensiones de memoria y FFT
#define N_PULSO_TX          32          // 32 muestras transmitidas (3.2 ms)
#define N_CAPTURA_RX        256         // 256 muestras capturadas (25.6 ms, ~4.3 metros maximo)
#define N_FFT_PUNTOS        512         // zero-padding para la fft radix-2 (512 >= 256 + 32 - 1)
#define N_CORRELACION       (N_CAPTURA_RX - N_PULSO_TX + 1) // 225 puntos de correlacion lineal

// Zona ciega y limites de deteccion
#define ZONA_CIEGA_MUESTRAS 22          // ignora las primeras 22 muestras (38 cm) para tapar el sonido directo del parlante
#define UMBRAL_CORR_MINIMO  500.0f      // nivel minimo de correlacion para validar un eco
#define MAX_DISTANCIA_CM    250.0f      // alcance maximo util del radar

// Configuracion de ejecucion
#define PERIODO_DISPARO_MS  2000        // 2000 ms para dar tiempo a transmitir el JSON completo a 115200 baudios
#define VELOCIDAD_SERIAL    115200      // baudios para el puerto serie

#endif // CONFIG_H
