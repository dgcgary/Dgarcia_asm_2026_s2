/*
 * Radar Acustico Monostatico - ESP32 (Modo Graficas)
 * Este programa envia tramas JSON con los datos del radar hacia el script
 * test/diagnostico_radar.py para visualizacion en tiempo real.
 */

#include <Arduino.h>
#include "driver/dac.h"
#include "config.h"
#include "dsp_radar.h"

static_assert(N_FFT_PUNTOS >= N_CAPTURA_RX + N_PULSO_TX - 1,
              "la correlacion lineal necesita suficiente zero-padding");

uint8_t dac_pulso_tx[N_PULSO_TX];
float   ref_pulso_tx[N_PULSO_TX];

uint16_t adc_raw_rx[N_CAPTURA_RX];
uint32_t ciclos_us[N_CAPTURA_RX];
float    senal_rx_dc[N_CAPTURA_RX];
float    senal_rx_filtrada[N_CAPTURA_RX];
float    corr_cruda[N_CORRELACION];
float    corr_filtrada[N_CORRELACION];

BiquadBandpass filtro_bp;

unsigned long ultimo_disparo_ms = 0;
unsigned long id_disparo = 0;

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

    // inicializa los bloques del pipeline DSP
    dsp_sintetizar_pulso(dac_pulso_tx, ref_pulso_tx, N_PULSO_TX, F0_SONAR_HZ, F1_SONAR_HZ, (float)FS_HZ);
    filtro_bp.init(2000.0f, 2000.0f, (float)FS_HZ);

    Serial.println("Radar Acustico Monostatico - ESP32 (Modo Graficas)");
    Serial.println("Abre el script test/diagnostico_radar.py para ver las senales en vivo.");
}

void loop() {
    // pausa configurada para dar tiempo a transmitir el JSON completo por el puerto serie
    if (millis() - ultimo_disparo_ms < PERIODO_DISPARO_MS) {
        return;
    }
    ultimo_disparo_ms = millis();
    id_disparo++;

    digitalWrite(PIN_LED_STATUS, HIGH);

    // 1. emite el pulso por el DAC y captura el ADC de forma sincronizada
    const unsigned long t_inicio_captura = micros();
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        const unsigned long t = micros();
        ciclos_us[n] = t - t_inicio_captura;

        if (n < N_PULSO_TX) {
            dac_output_voltage(DAC_CHANNEL_1, dac_pulso_tx[n]);
        } else {
            dac_output_voltage(DAC_CHANNEL_1, 128);
        }

        adc_raw_rx[n] = analogRead(PIN_RX_ADC);

        while (micros() - t < TS_US) {
            // espera activa precisa a 100 microsegundos
        }
    }
    const unsigned long captura_us = micros() - t_inicio_captura;
    dac_output_voltage(DAC_CHANNEL_1, 128);
    digitalWrite(PIN_LED_STATUS, LOW);

    const float fs_real = (float)N_CAPTURA_RX * 1000000.0f / (float)captura_us;

    // 2. elimina el offset DC y aplica el filtro pasa banda biquad
    float suma_adc = 0.0f;
    int saturadas = 0;
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        suma_adc += adc_raw_rx[n];
        if (adc_raw_rx[n] == 0 || adc_raw_rx[n] == 4095) {
            saturadas++;
        }
    }
    float media_dc = suma_adc / (float)N_CAPTURA_RX;

    filtro_bp.reset();
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        senal_rx_dc[n] = (float)adc_raw_rx[n] - media_dc;
        senal_rx_filtrada[n] = filtro_bp.process(senal_rx_dc[n]);
    }

    // 3. calcula la correlacion cruzada cruda y filtrada mediante FFT radix-2
    const unsigned long t_dsp_inicio = micros();
    dsp_correlacion_cruzada(senal_rx_dc, N_CAPTURA_RX, ref_pulso_tx, N_PULSO_TX, corr_cruda, N_FFT_PUNTOS);
    dsp_correlacion_cruzada(senal_rx_filtrada, N_CAPTURA_RX, ref_pulso_tx, N_PULSO_TX, corr_filtrada, N_FFT_PUNTOS);
    const unsigned long dsp_us = micros() - t_dsp_inicio;

    // 4. estima el retardo y calcula la distancia
    int m_pico = 0;
    float tof_ms = 0.0f;
    float dist_cm = 0.0f;
    float amp_pico = 0.0f;

    bool detectado = dsp_estimar_distancia(
        corr_filtrada, N_CORRELACION,
        ZONA_CIEGA_MUESTRAS, fs_real, VEL_SONIDO_MS, UMBRAL_CORR_MINIMO,
        m_pico, tof_ms, dist_cm, amp_pico
    );
    if (detectado) {
        dist_cm -= OFFSET_CALIBRACION_CM;
        if (dist_cm < 0.0f) dist_cm = 0.0f;
    }
    const bool fuera_rango = detectado && (dist_cm > MAX_DISTANCIA_CM);
    if (fuera_rango) {
        detectado = false;
    }

    // 5. transmite la trama JSON completa para la interfaz grafica en Python
    Serial.printf("{\"tipo\":\"radar6\",\"id\":%lu,\"fs_nominal\":%d,"
        "\"fs_real\":%.3f,\"captura_us\":%lu,\"dsp_us\":%lu,"
        "\"f0_nominal\":%.1f,\"f1_nominal\":%.1f,\"vel_sonido\":%.1f,"
        "\"zona_ciega\":%d,\"umbral\":%.1f,\"max_distancia_cm\":%.1f,"
        "\"media_adc\":%.2f,\"saturadas\":%d,\"pico\":%d,"
        "\"tof_ms\":%.5f,\"distancia_cm\":%.3f,\"amplitud\":%.3f,"
        "\"detectado\":%s,\"fuera_rango\":%s",
        id_disparo, FS_HZ, fs_real, captura_us, dsp_us,
        F0_SONAR_HZ, F1_SONAR_HZ, VEL_SONIDO_MS, ZONA_CIEGA_MUESTRAS,
        UMBRAL_CORR_MINIMO, MAX_DISTANCIA_CM, media_dc, saturadas, m_pico,
        tof_ms, dist_cm, amp_pico, detectado ? "true" : "false",
        fuera_rango ? "true" : "false");

    imprimir_array("tx", ref_pulso_tx, N_PULSO_TX);
    imprimir_array("rx", senal_rx_dc, N_CAPTURA_RX);
    imprimir_array("rx_filtrada", senal_rx_filtrada, N_CAPTURA_RX);
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
