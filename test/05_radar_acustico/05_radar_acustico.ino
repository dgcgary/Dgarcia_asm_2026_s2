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
float    senal_rx[N_CAPTURA_RX];
float    corr_out[N_CORRELACION];

unsigned long ultimo_disparo_ms = 0;

float dist_hist[HISTORIAL_DISTANCIAS];
int hist_idx = 0;
bool hist_lleno = false;

// -----------------------------
// Filtro pasa banda digital (Biquad) correcto.
// Se centra en 2.5-3 kHz para dejar pasar la banda útil del chirp y eliminar ruido.
// Esto es más estable que el filtro IIR "inventado" del intento anterior.
// -----------------------------
struct BiquadBandPass {
    float b0, b1, b2;
    float a1, a2;
    float x1, x2;
    float y1, y2;

    void init(float sampleRate, float centerHz, float bandwidthHz) {
        float Q = centerHz / bandwidthHz;  // aproximación práctica para la banda útil
        float w0 = 2.0f * PI * centerHz / sampleRate;
        float alpha = sinf(w0) / (2.0f * Q);

        b0 = alpha;
        b1 = 0.0f;
        b2 = -alpha;

        float a0 = 1.0f + alpha;
        a1 = -2.0f * cosf(w0);
        a2 = 1.0f - alpha;

        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;

        x1 = 0.0f;
        x2 = 0.0f;
        y1 = 0.0f;
        y2 = 0.0f;
    }

    float process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;

        return y;
    }
};

BiquadBandPass filtro_bp;

static void ordenar(float* a, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (a[j] < a[i]) {
                float tmp = a[i];
                a[i] = a[j];
                a[j] = tmp;
            }
        }
    }
}

static float mediana(float* v, int n) {
    float tmp[HISTORIAL_DISTANCIAS];
    for (int i = 0; i < n; i++) {
        tmp[i] = v[i];
    }
    ordenar(tmp, n);

    if (n % 2 == 1) {
        return tmp[n / 2];
    } else {
        return (tmp[n / 2 - 1] + tmp[n / 2]) * 0.5f;
    }
}

static float filtrar_distancia(float nueva_dist) {
    dist_hist[hist_idx] = nueva_dist;
    hist_idx++;
    if (hist_idx >= HISTORIAL_DISTANCIAS) {
        hist_idx = 0;
        hist_lleno = true;
    }

    int n = hist_lleno ? HISTORIAL_DISTANCIAS : hist_idx;
    return mediana(dist_hist, n);
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

    dsp_sintetizar_pulso(dac_pulso_tx, ref_pulso_tx, N_PULSO_TX, F0_SONAR_HZ, F1_SONAR_HZ, (float)FS_HZ);

    // Filtro pasa banda centrado en la banda útil del chirp
    filtro_bp.init((float)FS_HZ, 2500.0f, 5000.0f);

    Serial.println("\n=========================================================");
    Serial.println(" CE1110: RADAR ACUSTICO MONOSTATICO - ESP32 INICIALIZADO ");
    Serial.println("=========================================================");
    Serial.printf(" Frecuencia de muestreo (fs) : %d Hz (Ts = %d us)\n", FS_HZ, TS_US);
    Serial.printf(" Barrido de sondeo (chirp)   : %.0f Hz -> %.0f Hz\n", F0_SONAR_HZ, F1_SONAR_HZ);
    Serial.printf(" Duracion de pulso (Tx)      : %d muestras (%.2f ms)\n", N_PULSO_TX, (float)N_PULSO_TX / FS_HZ * 1000.0f);
    Serial.printf(" Ventana de escucha (Rx)     : %d muestras (%.2f ms)\n", N_CAPTURA_RX, (float)N_CAPTURA_RX / FS_HZ * 1000.0f);
    Serial.printf(" FFT Zero-Padding (N_FFT)    : %d puntos (Radix-2)\n", N_FFT_PUNTOS);
    Serial.printf(" Velocidad del sonido (vs)   : %.1f m/s\n", VEL_SONIDO_MS);
    Serial.printf(" Resolucion de distancia     : %.2f cm / muestra\n", (VEL_SONIDO_MS / (2.0f * FS_HZ)) * 100.0f);
    Serial.printf(" Zona ciega inicial          : %d muestras (~%.1f cm)\n", ZONA_CIEGA_MUESTRAS, (VEL_SONIDO_MS * ZONA_CIEGA_MUESTRAS / (2.0f * FS_HZ)) * 100.0f);
    Serial.printf(" Tiempo de captura esperado  : %lu us (%d muestras x %d us)\n", (unsigned long)(N_CAPTURA_RX * TS_US), N_CAPTURA_RX, TS_US);
    Serial.println("=========================================================");
    Serial.println("Coloca un obstaculo frente al sensor y observa la distancia en vivo...\n");
}

void loop() {
    if (millis() - ultimo_disparo_ms < PERIODO_DISPARO_MS) {
        return;
    }
    ultimo_disparo_ms = millis();

    digitalWrite(PIN_LED_STATUS, HIGH);

    // 1. Disparo de Pulso y Captura Sincronizada a 20 kHz
    unsigned long t_captura_inicio = micros();

    for (int n = 0; n < N_CAPTURA_RX; n++) {
        unsigned long t_inicio_muestra = micros();

        if (n < N_PULSO_TX) {
            dac_output_voltage(DAC_CHANNEL_1, dac_pulso_tx[n]);
        } else {
            dac_output_voltage(DAC_CHANNEL_1, 128);
        }

        adc_raw_rx[n] = analogRead(PIN_RX_ADC);

        while (micros() - t_inicio_muestra < TS_US) {
            // Espera activa a 50 us
        }
    }

    unsigned long t_captura_total_us = micros() - t_captura_inicio;
    dac_output_voltage(DAC_CHANNEL_1, 128);
    digitalWrite(PIN_LED_STATUS, LOW);

    // 2. Preprocesamiento: remover offset DC
    float suma_adc = 0.0f;
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        suma_adc += adc_raw_rx[n];
    }
    float media_dc = suma_adc / (float)N_CAPTURA_RX;

    for (int n = 0; n < N_CAPTURA_RX; n++) {
        senal_rx[n] = (float)adc_raw_rx[n] - media_dc;
    }

    // 3. Filtro pasa banda para dejar solo la banda útil del chirp
    for (int n = 0; n < N_CAPTURA_RX; n++) {
        senal_rx[n] = filtro_bp.process(senal_rx[n]);
    }

    // 4. DSP: Correlación cruzada vía FFT Radix-2
    unsigned long t_dsp_inicio = micros();
    dsp_correlacion_cruzada(senal_rx, N_CAPTURA_RX, ref_pulso_tx, N_PULSO_TX, corr_out, N_FFT_PUNTOS);
    unsigned long t_dsp_us = micros() - t_dsp_inicio;

    // 5. Estimación de retardo y distancia
    int m_pico = 0;
    float tau_ms = 0.0f;
    float distancia_cm = 0.0f;
    float amp_pico = 0.0f;

    float fs_real = (float)N_CAPTURA_RX / ((float)t_captura_total_us / 1000000.0f);

    bool eco_detectado = dsp_estimar_distancia(
        corr_out, N_CORRELACION,
        ZONA_CIEGA_MUESTRAS, fs_real, VEL_SONIDO_MS,
        UMBRAL_CORR_MINIMO,
        m_pico, tau_ms, distancia_cm, amp_pico
    );

    if (eco_detectado && (distancia_cm < 0.0f || distancia_cm > MAX_DISTANCIA_CM)) {
        eco_detectado = false;
        amp_pico = 0.0f;
        distancia_cm = 0.0f;
        tau_ms = 0.0f;
    }

    // 6. Salida serial formateada con mediana + outlier rejection
    if (eco_detectado) {
        float dist_filtrada = filtrar_distancia(distancia_cm);

        if (hist_lleno && fabsf(distancia_cm - dist_filtrada) > TOLERANCIA_OUTLIER_CM) {
            Serial.printf("[OUTLIER] Distancia descartada: %6.1f cm | Mediana: %6.1f cm | CorrAmp: %7.1f\n",
                          distancia_cm, dist_filtrada, amp_pico);
            return;
        }

        int barra_len = map((int)dist_filtrada, 15, 250, 1, 30);
        if (barra_len < 1) barra_len = 1;
        if (barra_len > 30) barra_len = 30;
        char barra[32];
        for (int b = 0; b < barra_len; b++) barra[b] = '=';
        barra[barra_len] = '\0';

        Serial.printf("[ECO DETECTADO] Distancia: %6.1f cm | ToF: %5.2f ms | Pico: %3d | CorrAmp: %7.1f | DSP: %4lu us | Captura: %5lu us | [%-30s]\n",
                      dist_filtrada, tau_ms, m_pico, amp_pico, t_dsp_us, t_captura_total_us, barra);
    } else {
        Serial.printf("[BUSCANDO...]   Distancia:  ---.- cm | ToF:  --.-- ms | CorrMax: %7.1f (Bajo umbral) | DSP: %4lu us | Captura: %5lu us\n",
                      amp_pico, t_dsp_us, t_captura_total_us);
    }
}