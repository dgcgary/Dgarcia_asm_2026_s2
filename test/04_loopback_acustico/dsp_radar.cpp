#include "dsp_radar.h"
#include <math.h>

void dsp_sintetizar_pulso(uint8_t* dac_out, float* ref_out, int n_puntos, float f0, float fs) {
    for (int n = 0; n < n_puntos; n++) {
        float t = (float)n / fs;
        // ventana de Hann para suavizar el pulso
        float ventana = 0.5f * (1.0f - cosf(2.0f * PI * n / (n_puntos - 1)));
        float tono = sinf(2.0f * PI * f0 * t);
        float senal_modulada = tono * ventana;

        ref_out[n] = senal_modulada;

        int val_dac = (int)(128.0f + 120.0f * senal_modulada);
        if (val_dac < 0) val_dac = 0;
        if (val_dac > 255) val_dac = 255;
        dac_out[n] = (uint8_t)val_dac;
    }
}

void dsp_fft_radix2(Complex* X, int N, bool es_inversa) {
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

    if (es_inversa) {
        float inv_N = 1.0f / (float)N;
        for (int i = 0; i < N; i++) {
            X[i].real *= inv_N;
            X[i].imag *= inv_N;
        }
    }
}

static Complex buf_fft_rx[N_FFT_PUNTOS];
static Complex buf_fft_tx[N_FFT_PUNTOS];
static Complex buf_fft_prod[N_FFT_PUNTOS];

void dsp_correlacion_cruzada(
    const float* senal_rx, int n_rx,
    const float* senal_tx, int n_tx,
    float* r_out, int n_fft
) {
    for (int i = 0; i < n_fft; i++) {
        buf_fft_rx[i] = (i < n_rx) ? Complex(senal_rx[i], 0.0f) : Complex(0.0f, 0.0f);
        buf_fft_tx[i] = (i < n_tx) ? Complex(senal_tx[i], 0.0f) : Complex(0.0f, 0.0f);
    }

    dsp_fft_radix2(buf_fft_rx, n_fft, false);
    dsp_fft_radix2(buf_fft_tx, n_fft, false);

    for (int k = 0; k < n_fft; k++) {
        buf_fft_prod[k] = buf_fft_rx[k] * buf_fft_tx[k].conj();
    }

    dsp_fft_radix2(buf_fft_prod, n_fft, true);

    int n_validas = n_rx - n_tx + 1;
    for (int m = 0; m < n_validas; m++) {
        r_out[m] = buf_fft_prod[m].real;
    }
}
