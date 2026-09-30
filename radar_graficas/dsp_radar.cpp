#include "dsp_radar.h"
#include <math.h>

void dsp_sintetizar_pulso(uint8_t* dac_out, float* ref_out, int n_puntos, float f0, float f1, float fs) {
    float duracion = (float)n_puntos / fs;
    float k = (f1 - f0) / duracion;  // tasa de barrido chirp en Hz/s

    for (int n = 0; n < n_puntos; n++) {
        float t = (float)n / fs;

        // ventana de Hann para suavizar los extremos del pulso acustico
        float ventana = 0.5f * (1.0f - cosf(2.0f * PI * n / (n_puntos - 1)));

        // fase instantanea del chirp
        float fase = 2.0f * PI * (f0 * t + 0.5f * k * t * t);
        float tono = sinf(fase);
        float senal_modulada = tono * ventana;

        // guarda la referencia normalizada para la correlacion cruzada
        ref_out[n] = senal_modulada;

        // cuantiza a 8 bits (0 a 255) centrada en 128 (1.65 V) para el DAC
        int val_dac = (int)(128.0f + 115.0f * senal_modulada);
        if (val_dac < 0) val_dac = 0;
        if (val_dac > 255) val_dac = 255;
        dac_out[n] = (uint8_t)val_dac;
    }
}

void dsp_fft_radix2(Complex* X, int N, bool es_inversa) {
    // 1. reordena por inversion de bits (bit-reversal)
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

    // 2. calcula las mariposas de Cooley-Tukey por etapas
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

    // 3. normaliza por 1/N si es la transformada inversa
    if (es_inversa) {
        float inv_N = 1.0f / (float)N;
        for (int i = 0; i < N; i++) {
            X[i].real *= inv_N;
            X[i].imag *= inv_N;
        }
    }
}

// buffers estaticos en memoria SRAM para evitar fragmentacion
static Complex buf_fft_rx[N_FFT_PUNTOS];
static Complex buf_fft_tx[N_FFT_PUNTOS];
static Complex buf_fft_prod[N_FFT_PUNTOS];

void dsp_correlacion_cruzada(
    const float* senal_rx, int n_rx,
    const float* senal_tx, int n_tx,
    float* r_out, int n_fft
) {
    // 1. carga la señal Rx rellenando con ceros hasta 512 puntos
    for (int i = 0; i < n_fft; i++) {
        buf_fft_rx[i] = (i < n_rx) ? Complex(senal_rx[i], 0.0f) : Complex(0.0f, 0.0f);
    }

    // 2. carga la señal Tx de referencia rellenando con ceros
    for (int i = 0; i < n_fft; i++) {
        buf_fft_tx[i] = (i < n_tx) ? Complex(senal_tx[i], 0.0f) : Complex(0.0f, 0.0f);
    }

    // 3. calcula la FFT directa de ambas señales
    dsp_fft_radix2(buf_fft_rx, n_fft, false);
    dsp_fft_radix2(buf_fft_tx, n_fft, false);

    // 4. multiplica en frecuencia: FFT(Rx) * conj(FFT(Tx))
    for (int k = 0; k < n_fft; k++) {
        buf_fft_prod[k] = buf_fft_rx[k] * buf_fft_tx[k].conj();
    }

    // 5. aplica la IFFT para obtener la correlacion en el dominio temporal
    dsp_fft_radix2(buf_fft_prod, n_fft, true);

    // 6. extrae unicamente las muestras lineales validas
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

bool dsp_estimar_distancia(
    const float* R, int n_corr,
    int zona_ciega, float fs, float vel_sonido,
    float umbral_min,
    int& m_pico, float& tau_ms, float& dist_cm, float& amp_pico
) {
    if (zona_ciega >= n_corr) return false;

    float max_val = -1e9f;
    int idx_max = -1;

    // busca el pico maximo despues de la zona ciega
    for (int m = zona_ciega; m < n_corr; m++) {
        float val = fabsf(R[m]);
        if (val > max_val) {
            max_val = val;
            idx_max = m;
        }
    }

    amp_pico = max_val;
    m_pico = idx_max;

    // comprueba si supera el umbral minimo de eco
    if (idx_max < 0 || max_val < umbral_min) {
        tau_ms = 0.0f;
        dist_cm = 0.0f;
        return false;
    }

    // tiempo de vuelo: tau = m_pico / fs
    tau_ms = ((float)m_pico / fs) * 1000.0f;

    // distancia monostatica: d = (vs * tau) / 2
    float dist_metros = (vel_sonido * ((float)m_pico / fs)) / 2.0f;
    dist_cm = dist_metros * 100.0f;

    return true;
}
