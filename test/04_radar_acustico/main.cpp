/**
 * ============================================================================
 * CE1110 - Análisis de Señales Mixtas | Instituto Tecnológico de Costa Rica
 * Proyecto: Radar Acústico Monostático (DSP Embebido en ESP32)
 * Archivo: main.cpp - Pipeline Completo de Transmisión, Captura y DSP
 * ============================================================================
 */

#include <Arduino.h>
#include "driver/dac.h"
#include "config.h"
#include "dsp_radar.h"

// Búferes de Transmisión (Sintetizados en setup)
uint8_t dac_pulso_tx[N_PULSO_TX];
float   ref_pulso_tx[N_PULSO_TX];

// Búferes de Recepción y Procesamiento
uint16_t adc_raw_rx[N_CAPTURA_RX];
float    senal_rx[N_CAPTURA_RX];
float    corr_out[N_CORRELACION];

unsigned long ultimo_disparo_ms = 0;

void setup() {
    Serial.begin(VELOCIDAD_SERIAL);
    delay(1000);

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    // 1. Configurar ADC (12 bits: 0 a 4095)
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_RX_ADC, ADC_11db);

    // 2. Habilitar DAC de hardware en GPIO 25
    dac_output_enable(DAC_CHANNEL_1);
    dac_output_voltage(DAC_CHANNEL_1, 128); // Nivel de reposo (1.65V)

    // 3. Sintetizar pulso acústico de sondeo (2.5 kHz con ventana Hann)
    dsp_sintetizar_pulso(dac_pulso_tx, ref_pulso_tx, N_PULSO_TX, FREQ_SONAR_HZ, (float)FS_HZ);

    Serial.println("\n=========================================================");
    Serial.println(" CE1110: RADAR ACUSTICO MONOSTATICO - ESP32 INICIALIZADO ");
    Serial.println("=========================================================");
    Serial.printf(" Frecuencia de muestreo (fs) : %d Hz (Ts = %d us)\n", FS_HZ, TS_US);
    Serial.printf(" Frecuencia de sondeo (f0)   : %.1f Hz\n", FREQ_SONAR_HZ);
    Serial.printf(" Duracion de pulso (Tx)      : %d muestras (%.2f ms)\n", N_PULSO_TX, (float)N_PULSO_TX / FS_HZ * 1000.0f);
    Serial.printf(" Ventana de escucha (Rx)     : %d muestras (%.2f ms)\n", N_CAPTURA_RX, (float)N_CAPTURA_RX / FS_HZ * 1000.0f);
    Serial.printf(" FFT Zero-Padding (N_FFT)    : %d puntos (Radix-2)\n", N_FFT_PUNTOS);
    Serial.printf(" Velocidad del sonido (vs)   : %.1f m/s\n", VEL_SONIDO_MS);
    Serial.printf(" Resolucion de distancia     : %.2f cm / muestra\n", (VEL_SONIDO_MS / (2.0f * FS_HZ)) * 100.0f);
    Serial.printf(" Zona ciega inicial          : %d muestras (~%.1f cm)\n", ZONA_CIEGA_MUESTRAS, (VEL_SONIDO_MS * ZONA_CIEGA_MUESTRAS / (2.0f * FS_HZ)) * 100.0f);
    Serial.println("=========================================================");
    Serial.println("Coloca un obstaculo frente al sensor y observa la distancia en vivo...\n");
}

void loop() {
    if (millis() - ultimo_disparo_ms < PERIODO_DISPARO_MS) {
        return;
    }
    ultimo_disparo_ms = millis();

    digitalWrite(PIN_LED_STATUS, HIGH);

    // ========================================================================
    // ETAPA 1: DISPARO Y ADQUISICIÓN SINCRONIZADA (Zero-Lag Software)
    // ========================================================================
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        unsigned long t_inicio_muestra = micros();

        // 1. Transmisión por DAC
        if (n < N_PULSO_TX) {
            dac_output_voltage(DAC_CHANNEL_1, dac_pulso_tx[n]);
        } else {
            dac_output_voltage(DAC_CHANNEL_1, 128); // Silencio (reposo en 1.65V)
        }

        // 2. Adquisición simultánea por ADC
        adc_raw_rx[n] = analogRead(PIN_RX_ADC);

        // 3. Cadencia de muestreo precisa (100 us)
        while (micros() - t_inicio_muestra < TS_US) {
            // Espera activa
        }
    }
    dac_output_voltage(DAC_CHANNEL_1, 128);

    digitalWrite(PIN_LED_STATUS, LOW);

    // ========================================================================
    // ETAPA 2: PREPROCESAMIENTO (Eliminación de Offset DC)
    // ========================================================================
    float suma_adc = 0.0f;
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        suma_adc += adc_raw_rx[n];
    }
    float media_dc = suma_adc / (float)N_CAPTURA_RX;

    for (int n = 0; n < N_CAPTURA_RX; n++) {
        senal_rx[n] = (float)adc_raw_rx[n] - media_dc;
    }

    // ========================================================================
    // ETAPA 3: PROCESAMIENTO DSP (Correlación Cruzada vía FFT Radix-2)
    // ========================================================================
    unsigned long t_dsp_inicio = micros();
    
    dsp_correlacion_cruzada(
        senal_rx, N_CAPTURA_RX,
        ref_pulso_tx, N_PULSO_TX,
        corr_out, N_FFT_PUNTOS
    );

    unsigned long t_dsp_us = micros() - t_dsp_inicio;

    // ========================================================================
    // ETAPA 4: ESTIMACIÓN DE RETARDO Y CÁLCULO DE DISTANCIA
    // ========================================================================
    int m_pico = 0;
    float tau_ms = 0.0f;
    float distancia_cm = 0.0f;
    float amp_pico = 0.0f;

    bool eco_detectado = dsp_estimar_distancia(
        corr_out, N_CORRELACION,
        ZONA_CIEGA_MUESTRAS, (float)FS_HZ, VEL_SONIDO_MS,
        UMBRAL_CORR_MINIMO,
        m_pico, tau_ms, distancia_cm, amp_pico
    );

    // ========================================================================
    // ETAPA 5: SALIDA SERIAL FORMATEADA
    // ========================================================================
    if (eco_detectado) {
        // Barra gráfica proporcional a la distancia (de 15 cm a 250 cm)
        int barra_len = map((int)distancia_cm, 15, 250, 1, 30);
        if (barra_len < 1) barra_len = 1;
        if (barra_len > 30) barra_len = 30;
        char barra[32];
        for (int b = 0; b < barra_len; b++) barra[b] = '=';
        barra[barra_len] = '\0';

        Serial.printf("[ECO DETECTADO] Distancia: %6.1f cm | ToF: %5.2f ms | Pico: %3d | CorrAmp: %7.1f | DSP: %4lu us | [%-30s]\n",
                      distancia_cm, tau_ms, m_pico, amp_pico, t_dsp_us, barra);
    } else {
        Serial.printf("[BUSCANDO...]   Distancia:  ---.- cm | ToF:  --.-- ms | CorrMax: %7.1f (Bajo umbral) | DSP: %4lu us\n",
                      amp_pico, t_dsp_us);
    }
}
