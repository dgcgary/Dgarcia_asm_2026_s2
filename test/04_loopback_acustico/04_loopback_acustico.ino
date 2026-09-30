/**
 * Test 04: Loopback acustico
 * Este test emite pulsos acusticos y los detecta mediante un microfono.
 * Mide la correlacion entre la señal emitida y la capturada para verificar el acople directo.
 */

#include "driver/dac.h"
#include "config.h"
#include "dsp_radar.h"

uint8_t dac_pulso_tx[N_PULSO_TX];
float   ref_pulso_tx[N_PULSO_TX];

uint16_t adc_raw_rx[N_CAPTURA_RX];
float    senal_rx[N_CAPTURA_RX];
float    corr_out[N_CORRELACION];

unsigned long ultimo_disparo_ms = 0;
int contador_pulsos = 0;

void setup() {
    Serial.begin(VELOCIDAD_SERIAL);
    delay(1000);

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_RX_ADC, ADC_11db);

    dac_output_enable(DAC_CHANNEL_1);
    dac_output_voltage(DAC_CHANNEL_1, 128);

    dsp_sintetizar_pulso(dac_pulso_tx, ref_pulso_tx, N_PULSO_TX, FREQ_SONAR_HZ, (float)FS_HZ);

    Serial.println("\nTest 04: Loopback acustico (deteccion de pulso propio)");
    Serial.println("El parlante emite pulsos cortos y el microfono captura el sonido.");
    Serial.println("La FFT calcula la correlacion para verificar el acople directo.\n");
}

void loop() {
    if (millis() - ultimo_disparo_ms < PERIODO_TEST_MS) {
        return;
    }
    ultimo_disparo_ms = millis();
    contador_pulsos++;

    digitalWrite(PIN_LED_STATUS, HIGH);

    // 1. emite el pulso y captura la senal en el ADC a 10 kHz
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        unsigned long t0 = micros();

        if (n < N_PULSO_TX) {
            dac_output_voltage(DAC_CHANNEL_1, dac_pulso_tx[n]);
        } else {
            dac_output_voltage(DAC_CHANNEL_1, 128);
        }

        adc_raw_rx[n] = analogRead(PIN_RX_ADC);

        while (micros() - t0 < TS_US) {
            // espera activa a 100 us
        }
    }
    dac_output_voltage(DAC_CHANNEL_1, 128);
    digitalWrite(PIN_LED_STATUS, LOW);

    // 2. remueve el offset DC de la captura
    float suma_adc = 0.0f;
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        suma_adc += adc_raw_rx[n];
    }
    float media_dc = suma_adc / (float)N_CAPTURA_RX;

    for (int n = 0; n < N_CAPTURA_RX; n++) {
        senal_rx[n] = (float)adc_raw_rx[n] - media_dc;
    }

    // 3. calcula la correlacion cruzada mediante FFT radix-2
    unsigned long t_dsp_0 = micros();
    dsp_correlacion_cruzada(senal_rx, N_CAPTURA_RX, ref_pulso_tx, N_PULSO_TX, corr_out, N_FFT_PUNTOS);
    unsigned long t_dsp_us = micros() - t_dsp_0;

    // 4. busca el pico maximo absoluto
    float max_corr = 0.0f;
    int m_pico = 0;
    for (int m = 0; m < N_CORRELACION; m++) {
        float val = fabsf(corr_out[m]);
        if (val > max_corr) {
            max_corr = val;
            m_pico = m;
        }
    }

    float retardo_ms = ((float)m_pico / (float)FS_HZ) * 1000.0f;

    // 5. imprime los datos y el perfil de correlacion
    Serial.println("----------------------------------------------------------------------");
    Serial.printf("[PULSO #%3d] Pico en muestra m = %2d (%4.2f ms) | Amplitud Corr: %6.1f | DSP: %lu us\n",
                  contador_pulsos, m_pico, retardo_ms, max_corr, t_dsp_us);

    Serial.print("Perfil Corr [m=0..34]: ");
    for (int m = 0; m < 35; m++) {
        float norm = (max_corr > 0.0f) ? (fabsf(corr_out[m]) / max_corr) : 0.0f;
        if (m == m_pico) {
            Serial.print("*"); // pico detectado
        } else if (norm > 0.6f) {
            Serial.print("#");
        } else if (norm > 0.3f) {
            Serial.print("-");
        } else {
            Serial.print(".");
        }
    }
    Serial.println();

    if (m_pico <= 8 && max_corr > 80.0f) {
        Serial.println(">>> [EXITO] Acople acustico directo detectado correctamente.");
    } else if (max_corr <= 80.0f) {
        Serial.println(">>> [AVISO] Pulso debil. Ajusta ligeramente el volumen del PAM8403.");
    } else {
        Serial.println(">>> [ECO DETECTADO]");
    }
}
