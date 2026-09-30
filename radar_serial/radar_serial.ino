/*
 * Radar Acustico Monostatico - ESP32 (Modo Serial)
 * imprime en el serial monitor las mediciones del radar.
 */

#include "driver/dac.h"
#include "config.h"
#include "dsp_radar.h"

uint8_t dac_pulso_tx[N_PULSO_TX];
float   ref_pulso_tx[N_PULSO_TX];

uint16_t adc_raw_rx[N_CAPTURA_RX];
float    senal_rx_dc[N_CAPTURA_RX];
float    senal_rx_filtrada[N_CAPTURA_RX];
float    corr_out[N_CORRELACION];

BiquadBandpass filtro_bp;
FiltroMediana  filtro_med;

unsigned long ultimo_disparo_ms = 0;
unsigned long contador_pulsos = 0;
int detecciones_consecutivas = 0;

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
    filtro_med.init(5);

    Serial.println("\nRadar Acustico Monostatico - ESP32 (Modo Serial)");
    Serial.printf("Frecuencia nominal: %d Hz (Ts = %d us)\n", FS_HZ, TS_US);
    Serial.printf("Pulso chirp: %.0f Hz -> %.0f Hz (%d muestras, %.2f ms)\n", 
                  F0_SONAR_HZ, F1_SONAR_HZ, N_PULSO_TX, (float)N_PULSO_TX / FS_HZ * 1000.0f);
    Serial.printf("Ventana de recepcion: %d muestras (%.2f ms)\n", 
                  N_CAPTURA_RX, (float)N_CAPTURA_RX / FS_HZ * 1000.0f);
    Serial.printf("Zona ciega: %d muestras (~%.1f cm)\n", 
                  ZONA_CIEGA_MUESTRAS, (VEL_SONIDO_MS * ZONA_CIEGA_MUESTRAS / (2.0f * FS_HZ)) * 100.0f);
    Serial.printf("Rango util: %.1f cm a %.1f cm\n\n", 
                  (VEL_SONIDO_MS * ZONA_CIEGA_MUESTRAS / (2.0f * FS_HZ)) * 100.0f, MAX_DISTANCIA_CM);
}

void loop() {
    if (millis() - ultimo_disparo_ms < PERIODO_DISPARO_MS) {
        return;
    }
    ultimo_disparo_ms = millis();
    contador_pulsos++;

    digitalWrite(PIN_LED_STATUS, HIGH);

    // 1. emite el pulso por el DAC y captura el ADC de forma sincronizada
    const unsigned long t_inicio_captura = micros();
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        unsigned long t_inicio_muestra = micros();

        if (n < N_PULSO_TX) {
            dac_output_voltage(DAC_CHANNEL_1, dac_pulso_tx[n]);
        } else {
            dac_output_voltage(DAC_CHANNEL_1, 128);
        }

        adc_raw_rx[n] = analogRead(PIN_RX_ADC);

        while (micros() - t_inicio_muestra < TS_US) {
            // espera activa precisa a 100 microsegundos
        }
    }
    const unsigned long captura_us = micros() - t_inicio_captura;
    dac_output_voltage(DAC_CHANNEL_1, 128);
    digitalWrite(PIN_LED_STATUS, LOW);

    // calcula la frecuencia de muestreo real medida
    const float fs_real = (float)N_CAPTURA_RX * 1000000.0f / (float)captura_us;

    // 2. elimina el offset DC y aplica el filtro pasa banda biquad
    float suma_adc = 0.0f;
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        suma_adc += adc_raw_rx[n];
    }
    float media_dc = suma_adc / (float)N_CAPTURA_RX;

    filtro_bp.reset();
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        senal_rx_dc[n] = (float)adc_raw_rx[n] - media_dc;
        senal_rx_filtrada[n] = filtro_bp.process(senal_rx_dc[n]);
    }

    // 3. calcula la correlacion cruzada mediante FFT radix-2 con la señal filtrada
    unsigned long t_dsp_inicio = micros();
    dsp_correlacion_cruzada(senal_rx_filtrada, N_CAPTURA_RX, ref_pulso_tx, N_PULSO_TX, corr_out, N_FFT_PUNTOS);
    unsigned long t_dsp_us = micros() - t_dsp_inicio;

    // 4. estima el retardo y calcula la distancia
    int m_pico = 0;
    float tau_ms = 0.0f;
    float dist_cruda_cm = 0.0f;
    float amp_pico = 0.0f;

    bool eco_detectado = dsp_estimar_distancia(
        corr_out, N_CORRELACION,
        ZONA_CIEGA_MUESTRAS, fs_real, VEL_SONIDO_MS,
        UMBRAL_CORR_MINIMO,
        m_pico, tau_ms, dist_cruda_cm, amp_pico
    );
    if (eco_detectado) {
        dist_cruda_cm -= OFFSET_CALIBRACION_CM;
        if (dist_cruda_cm < 0.0f) dist_cruda_cm = 0.0f;
    }
    if (eco_detectado && dist_cruda_cm > MAX_DISTANCIA_CM) {
        eco_detectado = false;
    }

    // 5. estabiliza la medicion con el filtro de mediana movil
    float dist_filtrada_cm = 0.0f;
    if (eco_detectado) {
        dist_filtrada_cm = filtro_med.actualizar(dist_cruda_cm);
        detecciones_consecutivas++;
    } else {
        detecciones_consecutivas = 0;
        filtro_med.reset();
    }

    // 6. imprime el resultado en el monitor serie
    if (eco_detectado) {
        int barra_len = map((int)dist_filtrada_cm, 35, (int)MAX_DISTANCIA_CM, 1, 25);
        if (barra_len < 1) barra_len = 1;
        if (barra_len > 25) barra_len = 25;
        char barra[28];
        for (int b = 0; b < barra_len; b++) barra[b] = '=';
        barra[barra_len] = '\0';

        Serial.printf("[ECO #%4lu] Distancia: %5.1f cm (Cruda: %5.1f cm) | ToF: %5.2f ms | Pico: %3d | Amp: %6.0f | DSP: %4lu us | [%-25s]\n",
                      contador_pulsos, dist_filtrada_cm, dist_cruda_cm, tau_ms, m_pico, amp_pico, t_dsp_us, barra);
    } else {
        Serial.printf("[PULSO #%4lu] BUSCANDO... (Sin eco > %.0f) | CorrMax: %6.0f | DSP: %4lu us\n",
                      contador_pulsos, UMBRAL_CORR_MINIMO, amp_pico, t_dsp_us);
    }
}
