/**
 * Test 02: Generador de audio DAC
 * Este test genera un tono de 1000 Hz utilizando el DAC del ESP32 en el pin 25.
 * Alterna entre 1 segundo de tono activo y 1 segundo de silencio.
 */

#include <Arduino.h>
#include <math.h>
#include "driver/dac.h"

#define PIN_DAC 25      // DAC_CHANNEL_1 en el ESP32
#define PIN_LED 2

const float FREQ_TONO = 1000.0f;        // 1000 Hz
const uint32_t FS = 20000;              // frecuencia de muestreo de 20 kHz
const uint32_t TS_US = 1000000 / FS;    // 50 us por muestra

const int N_PUNTOS = 20; // 20000 / 1000 = 20 muestras por ciclo
uint8_t tabla_seno[N_PUNTOS];

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(PIN_LED, OUTPUT);

    // habilita el DAC en GPIO 25
    dac_output_enable(DAC_CHANNEL_1);

    // precalcula la onda senoidal
    for (int i = 0; i < N_PUNTOS; i++) {
        float angulo = 2.0f * PI * i / N_PUNTOS;
        tabla_seno[i] = (uint8_t)(128 + 120 * sin(angulo));
    }

    Serial.println("\nTest 02: Generador de audio DAC (GPIO 25)");
    Serial.println("Emite un tono de 1000 Hz alternando 1 segundo activo y 1 segundo en silencio.\n");
}

void loop() {
    static unsigned long t_ciclo = 0;
    static int idx = 0;

    // alterna 1 segundo con tono y 1 segundo en silencio
    bool transmitiendo = (millis() % 2000) < 1000;

    unsigned long t0 = micros();

    if (transmitiendo) {
        digitalWrite(PIN_LED, HIGH);
        dac_output_voltage(DAC_CHANNEL_1, tabla_seno[idx]);
        idx = (idx + 1) % N_PUNTOS;
    } else {
        digitalWrite(PIN_LED, LOW);
        dac_output_voltage(DAC_CHANNEL_1, 128); // voltaje medio de silencio
    }

    if (millis() - t_ciclo >= 1000) {
        t_ciclo = millis();
        if (transmitiendo) {
            Serial.println("[DAC ACTIVO] -> emitiendo tono de 1000 Hz...");
        } else {
            Serial.println("[DAC SILENCIO] -> pausa...");
        }
    }

    while (micros() - t0 < TS_US) {
        // mantiene la cadencia de 20 kHz
    }
}
