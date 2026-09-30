#ifndef DSP_RADAR_H
#define DSP_RADAR_H

#include "config.h"
#include <stdint.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Representa numeros complejos para el calculo de la FFT
struct Complex {
    float real;
    float imag;

    Complex() : real(0.0f), imag(0.0f) {}
    Complex(float r, float i) : real(r), imag(i) {}

    inline Complex operator+(const Complex& b) const {
        return Complex(real + b.real, imag + b.imag);
    }
    inline Complex operator-(const Complex& b) const {
        return Complex(real - b.real, imag - b.imag);
    }
    inline Complex operator*(const Complex& b) const {
        return Complex(real * b.real - imag * b.imag, real * b.imag + imag * b.real);
    }
    inline Complex conj() const {
        return Complex(real, -imag);
    }
};

// Filtro digital pasa banda IIR biquad de 2do orden para limpiar la señal del microfono
struct BiquadBandpass {
    float b0, b2, a1, a2;
    float x1, x2, y1, y2;

    void init(float f_centro = 2000.0f, float ancho_banda = 2000.0f, float fs = 10000.0f);
    float process(float x);
    void reset();
};

// Sintetiza el pulso chirp lineal con ventana Hann en memoria
void dsp_sintetizar_pulso(uint8_t* dac_out, float* ref_out, int n_puntos, float f0, float f1, float fs);

// Ejecuta la FFT directa o inversa usando el algoritmo Cooley-Tukey radix-2
void dsp_fft_radix2(Complex* X, int N, bool es_inversa = false);

// Calcula la correlacion cruzada lineal en frecuencia usando FFT con zero-padding
void dsp_correlacion_cruzada(
    const float* senal_rx, int n_rx,
    const float* senal_tx, int n_tx,
    float* r_out, int n_fft
);

// Busca el pico maximo fuera de la zona ciega y calcula el tiempo de vuelo y distancia
bool dsp_estimar_distancia(
    const float* R, int n_corr,
    int zona_ciega, float fs, float vel_sonido,
    float umbral_min,
    int& m_pico, float& tau_ms, float& dist_cm, float& amp_pico
);

#endif // DSP_RADAR_H
