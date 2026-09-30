#ifndef DSP_RADAR_H
#define DSP_RADAR_H

#include "config.h"
#include <stdint.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// numeros complejos para calculo de la FFT
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

void dsp_sintetizar_pulso(uint8_t* dac_out, float* ref_out, int n_puntos, float f0, float fs);
void dsp_fft_radix2(Complex* X, int N, bool es_inversa = false);
void dsp_correlacion_cruzada(
    const float* senal_rx, int n_rx,
    const float* senal_tx, int n_tx,
    float* r_out, int n_fft
);

#endif // DSP_RADAR_H
