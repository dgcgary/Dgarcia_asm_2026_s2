/**
 * ============================================================================
 * CE1110 - Análisis de Señales Mixtas | Instituto Tecnológico de Costa Rica
 * Proyecto: Radar Acústico Monostático (DSP Embebido en ESP32)
 * Archivo: 05_radar_acustico.ino - Firmware Completo
 * ============================================================================
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

    // Inicialización del pipeline DSP
    dsp_sintetizar_pulso(dac_pulso_tx, ref_pulso_tx, N_PULSO_TX, F0_SONAR_HZ, F1_SONAR_HZ, (float)FS_HZ);
    filtro_bp.init(2000.0f, 2000.0f, (float)FS_HZ);
    filtro_med.init(5);

    Serial.println("\n=========================================================");
    Serial.println(" CE1110: RADAR ACUSTICO MONOSTATICO - ESP32 EMBEBIDO     ");
    Serial.println("=========================================================");
    Serial.printf(" Frecuencia de muestreo (fs) : %d Hz (Ts = %d us)\n", FS_HZ, TS_US);
    Serial.printf(" Barrido de sondeo (chirp)   : %.0f Hz -> %.0f Hz (Hann)\n", F0_SONAR_HZ, F1_SONAR_HZ);
    Serial.printf(" Duracion de pulso (Tx)      : %d muestras (%.2f ms)\n", N_PULSO_TX, (float)N_PULSO_TX / FS_HZ * 1000.0f);
    Serial.printf(" Ventana de escucha (Rx)     : %d muestras (%.2f ms)\n", N_CAPTURA_RX, (float)N_CAPTURA_RX / FS_HZ * 1000.0f);
    Serial.printf(" FFT Zero-Padding (N_FFT)    : %d puntos (Radix-2)\n", N_FFT_PUNTOS);
    Serial.printf(" Filtro Pasa Banda           : Biquad IIR 2do Orden (1-3 kHz)\n");
    Serial.printf(" Filtro de Estabilidad       : Mediana Movil (5 muestras)\n");
    Serial.printf(" Velocidad del sonido (vs)   : %.1f m/s\n", VEL_SONIDO_MS);
    Serial.printf(" Zona ciega inicial          : %d muestras (~%.1f cm)\n", ZONA_CIEGA_MUESTRAS, (VEL_SONIDO_MS * ZONA_CIEGA_MUESTRAS / (2.0f * FS_HZ)) * 100.0f);
    Serial.printf(" Rango util                  : %.1f cm a %.1f cm\n", (VEL_SONIDO_MS * ZONA_CIEGA_MUESTRAS / (2.0f * FS_HZ)) * 100.0f, MAX_DISTANCIA_CM);
    Serial.println("=========================================================\n");
}

void loop() {
    if (millis() - ultimo_disparo_ms < PERIODO_DISPARO_MS) {
        return;
    }
    ultimo_disparo_ms = millis();
    contador_pulsos++;

    digitalWrite(PIN_LED_STATUS, HIGH);

    // 1. Disparo de Pulso y Captura Sincronizada a 10 kHz
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
            // Espera activa precisa a 100 us
        }
    }
    const unsigned long captura_us = micros() - t_inicio_captura;
    dac_output_voltage(DAC_CHANNEL_1, 128);
    digitalWrite(PIN_LED_STATUS, LOW);

    const float fs_real = (float)N_CAPTURA_RX * 1000000.0f / (float)captura_us;

    // 2. Preprocesamiento: Remover Offset DC y Filtrar con Biquad Pasa Banda
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

    // 3. DSP: Correlación Cruzada vía FFT Radix-2
    unsigned long t_dsp_inicio = micros();
    dsp_correlacion_cruzada(senal_rx_filtrada, N_CAPTURA_RX, ref_pulso_tx, N_PULSO_TX, corr_out, N_FFT_PUNTOS);
    unsigned long t_dsp_us = micros() - t_dsp_inicio;

    // 4. Estimación de Retardo y Cálculo de Distancia
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
    if (eco_detectado && dist_cruda_cm > MAX_DISTANCIA_CM) {
        eco_detectado = false;
    }

    // 5. Filtrado de Estabilidad por Mediana Móvil
    float dist_filtrada_cm = 0.0f;
    if (eco_detectado) {
        dist_filtrada_cm = filtro_med.actualizar(dist_cruda_cm);
        detecciones_consecutivas++;
    } else {
        detecciones_consecutivas = 0;
        filtro_med.reset();
    }

    // 6. Salida Serial Formateada
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
