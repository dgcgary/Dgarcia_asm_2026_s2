#include <Arduino.h>
#include <math.h>
#include "driver/dac.h"  // Driver nativo del ESP32 para el DAC

#define PIN_DAC 25      // DAC_CHANNEL_1 en el ESP32
#define PIN_LED 2

const float FREQ_TONO = 1000.0f;        // 1000 Hz (Tono claro y muy audible)
const uint32_t FS = 20000;              // Frecuencia de muestreo 20 kHz
const uint32_t TS_US = 1000000 / FS;    // 50 us por muestra

const int N_PUNTOS = 20; // 20000 / 1000 = 20 muestras por ciclo
uint8_t tabla_seno[N_PUNTOS];

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(PIN_LED, OUTPUT);

    // 1. Habilitar explícitamente el DAC de silicio en GPIO 25
    dac_output_enable(DAC_CHANNEL_1);

    // 2. Precalcular onda senoidal a MÁXIMA AMPLITUD (de 5 a 250)
    for (int i = 0; i < N_PUNTOS; i++) {
        float angulo = 2.0f * PI * i / N_PUNTOS;
        tabla_seno[i] = (uint8_t)(128 + 120 * sin(angulo));
    }

    Serial.println("==========================================================");
    Serial.println("=== TEST 02: Generador de Audio DAC1 (GPIO 25) ACTIVO ===");
    Serial.println("==========================================================");
    Serial.println("- Tono: 1000 Hz (1 segundo de sonido, 1 segundo de silencio)");
    Serial.println("- Amplitud: Maxima (0 a 3.3V pico a pico)");
    Serial.println("- Si no suena, revisa:");
    Serial.println("  1. Gira la perilla del PAM8403 en sentido horario (debe hacer 'clic').");
    Serial.println("  2. PAM8403 '+' conectado a V5 (5V) y '-' a GND.");
    Serial.println("  3. Capacitor 10uF: polo (+) en GPIO 25 y polo (-) en pin L.");
    Serial.println("  4. Pin G del PAM8403 conectado a GND.");
    Serial.println("==========================================================\n");
}

void loop() {
    static unsigned long t_ciclo = 0;
    static int idx = 0;

    // Alternar: 1 segundo transmitiendo tono, 1 segundo en silencio
    bool transmitiendo = (millis() % 2000) < 1000;

    unsigned long t0 = micros();

    if (transmitiendo) {
        digitalWrite(PIN_LED, HIGH);
        // Escribir muestra por driver nativo
        dac_output_voltage(DAC_CHANNEL_1, tabla_seno[idx]);
        idx = (idx + 1) % N_PUNTOS;
    } else {
        digitalWrite(PIN_LED, LOW);
        dac_output_voltage(DAC_CHANNEL_1, 128); // Nivel DC medio de silencio
    }

    // Mensaje por Serial cada 1 segundo
    if (millis() - t_ciclo >= 1000) {
        t_ciclo = millis();
        if (transmitiendo) {
            Serial.println("[DAC ACTIVO] -> Emitiendo tono de 1000 Hz...");
        } else {
            Serial.println("[DAC SILENCIO] -> Pausa...");
        }
    }

    while (micros() - t0 < TS_US) {
        // Mantener 20 kHz
    }
}
