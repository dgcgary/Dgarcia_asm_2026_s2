/**
 * Test 06: Diagnostico del radar acustico
 * Este test permite diagnosticar el funcionamiento del radar acustico implementado en el test 5.
 * Envía los datos completos en formato JSON para su análisis y visualización en Python.
 */

#include <Arduino.h>
#include "driver/dac.h"
#include "../05_radar_acustico/config.h"
#include "../05_radar_acustico/dsp_radar.h"

// diagnostico del test 5: envia los datos completos en JSON para graficar en Python
// no aplica filtro de mediana porque interesa observar cada disparo individualmente
static_assert(N_FFT_PUNTOS >= N_CAPTURA_RX + N_PULSO_TX - 1,
              "la correlacion lineal necesita suficiente zero-padding");

uint8_t tx_dac[N_PULSO_TX];
float tx_ref[N_PULSO_TX];
uint16_t rx_adc[N_CAPTURA_RX];
uint32_t ciclos_us[N_CAPTURA_RX];
float rx_dc[N_CAPTURA_RX], rx_filtrada[N_CAPTURA_RX];
float corr_cruda[N_CORRELACION], corr_filtrada[N_CORRELACION];
unsigned long ultimo_disparo = 0;
unsigned long id_disparo = 0;

// filtro biquad pasa banda para la senal de recepcion
struct BandPass {
    float b0, b2, a1, a2;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void init() {
        const float q = 2500.0f / 5000.0f;
        const float w = 2.0f * PI * 2500.0f / FS_HZ;
        const float alpha = sinf(w) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = alpha / a0;
        b2 = -alpha / a0;
        a1 = -2.0f * cosf(w) / a0;
        a2 = (1.0f - alpha) / a0;
        x1 = x2 = y1 = y2 = 0;
    }
    float process(float x) {
        float y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

void imprimir_array(const char* nombre, const float* valores, int n) {
    Serial.printf(",\"%s\":[", nombre);
    for (int i = 0; i < n; ++i) {
        if (i) Serial.print(',');
        Serial.print(valores[i], 5);
    }
    Serial.print(']');
}

void setup() {
    Serial.begin(VELOCIDAD_SERIAL);
    delay(1000);
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_RX_ADC, ADC_11db);
    dac_output_enable(DAC_CHANNEL_1);
    dac_output_voltage(DAC_CHANNEL_1, 128);
    dsp_sintetizar_pulso(tx_dac, tx_ref, N_PULSO_TX,
                        F0_SONAR_HZ, F1_SONAR_HZ, (float)FS_HZ);
    Serial.println("TEST 6: abrir test/diagnostico_radar.py para ver las graficas.");
}

void loop() {
    // pausa de 2 segundos para dar tiempo a transmitir el JSON completo por serial
    if (millis() - ultimo_disparo < 2000) return;
    ultimo_disparo = millis();
    digitalWrite(PIN_LED_STATUS, HIGH);
    const unsigned long inicio = micros();
    for (int n = 0; n < N_CAPTURA_RX; ++n) {
        const unsigned long t = micros();
        ciclos_us[n] = t - inicio;
        dac_output_voltage(DAC_CHANNEL_1, n < N_PULSO_TX ? tx_dac[n] : 128);
        rx_adc[n] = analogRead(PIN_RX_ADC);
        while (micros() - t < TS_US) {}
    }
    const unsigned long captura_us = micros() - inicio;
    dac_output_voltage(DAC_CHANNEL_1, 128);
    digitalWrite(PIN_LED_STATUS, LOW);

    float media = 0;
    int saturadas = 0;
    for (int n = 0; n < N_CAPTURA_RX; ++n) {
        media += rx_adc[n];
        if (rx_adc[n] == 0 || rx_adc[n] == 4095) ++saturadas;
    }
    media /= N_CAPTURA_RX;
    BandPass filtro;
    filtro.init();
    for (int n = 0; n < N_CAPTURA_RX; ++n) {
        rx_dc[n] = rx_adc[n] - media;
        rx_filtrada[n] = filtro.process(rx_dc[n]);
    }

    const unsigned long inicio_dsp = micros();
    dsp_correlacion_cruzada(rx_dc, N_CAPTURA_RX, tx_ref, N_PULSO_TX,
                           corr_cruda, N_FFT_PUNTOS);
    dsp_correlacion_cruzada(rx_filtrada, N_CAPTURA_RX, tx_ref, N_PULSO_TX,
                           corr_filtrada, N_FFT_PUNTOS);
    const unsigned long dsp_us = micros() - inicio_dsp;
    const float fs_real = N_CAPTURA_RX * 1000000.0f / captura_us;
    int pico = 0;
    float tof_ms = 0, distancia_cm = 0, amplitud = 0;
    bool detectado = dsp_estimar_distancia(corr_filtrada, N_CORRELACION,
        ZONA_CIEGA_MUESTRAS, fs_real, VEL_SONIDO_MS, UMBRAL_CORR_MINIMO,
        pico, tof_ms, distancia_cm, amplitud);
    const bool fuera_rango = detectado && distancia_cm > MAX_DISTANCIA_CM;
    if (fuera_rango) detectado = false;

    // no imprime nada durante la adquisicion; envia el JSON al terminar el DSP
    Serial.printf("{\"tipo\":\"radar6\",\"id\":%lu,\"fs_nominal\":%d,"
        "\"fs_real\":%.3f,\"captura_us\":%lu,\"dsp_us\":%lu,"
        "\"f0_nominal\":%.1f,\"f1_nominal\":%.1f,\"vel_sonido\":%.1f,"
        "\"zona_ciega\":%d,\"umbral\":%.1f,\"max_distancia_cm\":%.1f,"
        "\"media_adc\":%.2f,\"saturadas\":%d,\"pico\":%d,"
        "\"tof_ms\":%.5f,\"distancia_cm\":%.3f,\"amplitud\":%.3f,"
        "\"detectado\":%s,\"fuera_rango\":%s",
        ++id_disparo, FS_HZ, fs_real, captura_us, dsp_us,
        F0_SONAR_HZ, F1_SONAR_HZ, VEL_SONIDO_MS, ZONA_CIEGA_MUESTRAS,
        UMBRAL_CORR_MINIMO, MAX_DISTANCIA_CM, media, saturadas, pico,
        tof_ms, distancia_cm, amplitud, detectado ? "true" : "false",
        fuera_rango ? "true" : "false");
    imprimir_array("tx", tx_ref, N_PULSO_TX);
    imprimir_array("rx", rx_dc, N_CAPTURA_RX);
    imprimir_array("rx_filtrada", rx_filtrada, N_CAPTURA_RX);
    imprimir_array("corr", corr_cruda, N_CORRELACION);
    imprimir_array("corr_filtrada", corr_filtrada, N_CORRELACION);
    Serial.print(",\"ciclos_us\":[");
    for (int n = 0; n < N_CAPTURA_RX; ++n) {
        if (n) Serial.print(',');
        Serial.print(ciclos_us[n]);
    }
    Serial.println("]}");
    Serial.flush();
}
