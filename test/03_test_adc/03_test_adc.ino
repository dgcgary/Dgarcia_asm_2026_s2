#include <Arduino.h>

#define PIN_ADC 34
#define PIN_LED 2

const int N_MUESTRAS_VENTANA = 200; // 200 muestras a 10 kHz = ventana de 20 ms

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(PIN_LED, OUTPUT);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_ADC, ADC_11db);

    Serial.println("\n========================================================");
    Serial.println("=== TEST 03: MONITOR DE AUDIO EN VIVO (SOLO GPIO 34) ===");
    Serial.println("========================================================");
    Serial.println("Muestreo: 10 kHz | Analizando amplitud pico a pico (Vpp)");
    Serial.println("Habla, silba o golpea cerca del microfono para ver la barra.\n");
}

void loop() {
    int v_min = 4095;
    int v_max = 0;
    long suma = 0;

    // Capturar una ráfaga de 20 ms de audio a 10 kHz (100 us por muestra)
    for (int i = 0; i < N_MUESTRAS_VENTANA; i++) {
        unsigned long t0 = micros();
        int muestra = analogRead(PIN_ADC);

        suma += muestra;
        if (muestra < v_min) v_min = muestra;
        if (muestra > v_max) v_max = muestra;

        while (micros() - t0 < 100) {
            // Mantiene 10 kHz exactos
        }
    }

    // Cálculos de la ventana de audio
    float media_adc = (float)suma / N_MUESTRAS_VENTANA;
    float v_offset = (media_adc / 4095.0f) * 3.3f;
    int amplitud_pico_a_pico = v_max - v_min;
    float vpp = (amplitud_pico_a_pico / 4095.0f) * 3.3f;

    // Barra de intensidad sonora (amplitud pico a pico)
    int len_barra = map(amplitud_pico_a_pico, 0, 1500, 0, 35);
    if (len_barra > 35) len_barra = 35;
    char barra[40];
    for (int b = 0; b < len_barra; b++) barra[b] = '#';
    barra[len_barra] = '\0';

    // Salida por Serial
    Serial.printf("Offset DC: %.2fV | Vpp (Sonido): %4.2fV (%4d pts) | [%-35s]\n",
                  v_offset, vpp, amplitud_pico_a_pico, barra);

    // Parpadeo de LED si detecta sonido significativo (Vpp > 0.10 V)
    digitalWrite(PIN_LED, (vpp > 0.10f) ? HIGH : LOW);

    delay(20);
}
