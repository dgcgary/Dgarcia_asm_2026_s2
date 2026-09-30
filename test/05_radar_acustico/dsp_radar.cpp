/**
 * ============================================================================
 * CE1110 - Análisis de Señales Mixtas | Instituto Tecnológico de Costa Rica
 * Proyecto: Radar Acústico Monostático
 * Archivo: dsp_radar.cpp - Implementación del Pipeline DSP Embebido
 * ============================================================================
 */

#include "dsp_radar.h"
#include <math.h>

void dsp_sintetizar_pulso(uint8_t* dac_out, float* ref_out, int n_puntos, float f0, float f1, float fs) {
    float duracion = (float)n_puntos / fs;
    float k = (f1 - f0) / duracion;  // tasa de barrido (chirp rate) en Hz/s

    for (int n = 0; n < n_puntos; n++) {
        float t = (float)n / fs;

        // Ventana de Hann para suavizar el inicio y fin del pulso acústico
        float ventana = 0.5f * (1.0f - cosf(2.0f * PI * n / (n_puntos - 1)));

        // Fase instantanea del chirp: phi(t) = 2*pi*(f0*t + 0.5*k*t^2)
        float fase = 2.0f * PI * (f0 * t + 0.5f * k * t * t);
        float tono = sinf(fase);
        float senal_modulada = tono * ventana;

        // Referencia normalizada para correlación cruzada
        ref_out[n] = senal_modulada;

        // Cuantización a 8 bits (0 a 255) centrada en 128 (1.65V) para el DAC
        int val_dac = (int)(128.0f + 115.0f * senal_modulada);
        if (val_dac < 0) val_dac = 0;
        if (val_dac > 255) val_dac = 255;
        dac_out[n] = (uint8_t)val_dac;
    }
}

void dsp_fft_radix2(Complex* X, int N, bool es_inversa) {
    // 1. Reordenamiento Bit-Reversal
    int j = 0;
    for (int i = 0; i < N - 1; i++) {
        if (i < j) {
            Complex temp = X[i];
            X[i] = X[j];
            X[j] = temp;
        }
        int k = N >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    // 2. Cálculo de Mariposas Cooley-Tukey por Etapas
    for (int len = 2; len <= N; len <<= 1) {
        float angulo = 2.0f * PI / len * (es_inversa ? 1.0f : -1.0f);
        Complex wlen(cosf(angulo), sinf(angulo));
        int mitad = len >> 1;

        for (int i = 0; i < N; i += len) {
            Complex w(1.0f, 0.0f);
            for (int k = 0; k < mitad; k++) {
                Complex u = X[i + k];
                Complex v = X[i + k + mitad] * w;
                X[i + k] = u + v;
                X[i + k + mitad] = u - v;
                w = w * wlen;
            }
        }
    }

    // 3. Normalización 1/N para la Transformada Inversa
    if (es_inversa) {
        float inv_N = 1.0f / (float)N;
        for (int i = 0; i < N; i++) {
            X[i].real *= inv_N;
            X[i].imag *= inv_N;
        }
    }
}

// Búferes estáticos en memoria SRAM para evitar fragmentación
static Complex buf_fft_rx[N_FFT_PUNTOS];
static Complex buf_fft_tx[N_FFT_PUNTOS];
static Complex buf_fft_prod[N_FFT_PUNTOS];

void dsp_correlacion_cruzada(
    const float* senal_rx, int n_rx,
    const float* senal_tx, int n_tx,
    float* r_out, int n_fft
) {
    // 1. Cargar señal Rx con Zero-Padding
    for (int i = 0; i < n_fft; i++) {
        buf_fft_rx[i] = (i < n_rx) ? Complex(senal_rx[i], 0.0f) : Complex(0.0f, 0.0f);
    }

    // 2. Cargar señal Tx de referencia con Zero-Padding
    for (int i = 0; i < n_fft; i++) {
        buf_fft_tx[i] = (i < n_tx) ? Complex(senal_tx[i], 0.0f) : Complex(0.0f, 0.0f);
    }

    // 3. FFT hacia adelante de ambas señales
    dsp_fft_radix2(buf_fft_rx, n_fft, false);
    dsp_fft_radix2(buf_fft_tx, n_fft, false);

    // 4. Multiplicación espectral: FFT(Rx) * conj(FFT(Tx))
    for (int k = 0; k < n_fft; k++) {
        buf_fft_prod[k] = buf_fft_rx[k] * buf_fft_tx[k].conj();
    }

    // 5. IFFT para obtener la correlación cruzada en el tiempo
    dsp_fft_radix2(buf_fft_prod, n_fft, true);

    // 6. Extraer las muestras lineales válidas (R[0 ... N_rx - N_tx])
    int n_validas = n_rx - n_tx + 1;
    for (int m = 0; m < n_validas; m++) {
        r_out[m] = buf_fft_prod[m].real;
    }
}

void BiquadBandpass::init(float f_centro, float ancho_banda, float fs) {
    float q = f_centro / ancho_banda;
    float w0 = 2.0f * PI * f_centro / fs;
    float alpha = sinf(w0) / (2.0f * q);
    float a0 = 1.0f + alpha;

    b0 = alpha / a0;
    b2 = -alpha / a0;
    a1 = -2.0f * cosf(w0) / a0;
    a2 = (1.0f - alpha) / a0;

    reset();
}

void BiquadBandpass::reset() {
    x1 = 0.0f;
    x2 = 0.0f;
    y1 = 0.0f;
    y2 = 0.0f;
}

float BiquadBandpass::process(float x) {
    float y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
}

void FiltroMediana::init(int n_ventana) {
    if (n_ventana > VENTANA_MAX) n_ventana = VENTANA_MAX;
    if (n_ventana < 3) n_ventana = 3;
    tamano = n_ventana;
    reset();
}

void FiltroMediana::reset() {
    indice = 0;
    llenos = 0;
    for (int i = 0; i < VENTANA_MAX; i++) {
        buffer[i] = 0.0f;
    }
}

float FiltroMediana::actualizar(float nueva_distancia) {
    buffer[indice] = nueva_distancia;
    indice = (indice + 1) % tamano;
    if (llenos < tamano) llenos++;

    // Ordenamiento por inserción sobre una copia local
    float temp[VENTANA_MAX];
    for (int i = 0; i < llenos; i++) {
        temp[i] = buffer[i];
    }
    for (int i = 1; i < llenos; i++) {
        float clave = temp[i];
        int j = i - 1;
        while (j >= 0 && temp[j] > clave) {
            temp[j + 1] = temp[j];
            j--;
        }
        temp[j + 1] = clave;
    }

    return temp[llenos / 2];
}

bool dsp_estimar_distancia(
    const float* R, int n_corr,
    int zona_ciega, float fs, float vel_sonido,
    float umbral_min,
    int& m_pico, float& tau_ms, float& dist_cm, float& amp_pico
) {
    if (zona_ciega >= n_corr) return false;

    float max_val = -1e9f;
    int idx_max = -1;

    // Búsqueda del pico absoluto después de la zona ciega
    for (int m = zona_ciega; m < n_corr; m++) {
        float val = fabsf(R[m]);
        if (val > max_val) {
            max_val = val;
            idx_max = m;
        }
    }

    amp_pico = max_val;
    m_pico = idx_max;

    // Verificar si el pico supera el umbral de detección de eco
    if (idx_max < 0 || max_val < umbral_min) {
        tau_ms = 0.0f;
        dist_cm = 0.0f;
        return false;
    }

    // Tiempo de vuelo en milisegundos: tau = m_pico / fs
    tau_ms = ((float)m_pico / fs) * 1000.0f;

    // Distancia monostática: d = (v_s * tau) / 2
    float dist_metros = (vel_sonido * ((float)m_pico / fs)) / 2.0f;
    dist_cm = dist_metros * 100.0f;

    return true;
}
