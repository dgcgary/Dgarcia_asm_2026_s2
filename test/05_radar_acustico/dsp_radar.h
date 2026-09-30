/**
 * ============================================================================
 * CE1110 - Análisis de Señales Mixtas | Instituto Tecnológico de Costa Rica
 * Proyecto: Radar Acústico Monostático
 * Archivo: dsp_radar.h - Declaraciones del Pipeline DSP Embebido
 * ============================================================================
 */

#ifndef DSP_RADAR_H
#define DSP_RADAR_H

#include "config.h"
#include <stdint.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Estructura de número complejo para procesamiento DSP
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

/**
 * Sintetiza la señal de sondeo acústico s[n] como un CHIRP LINEAL
 * (barrido de frecuencia de f0 a f1) con modulación Hann para evitar
 * transitorios bruscos al activar/desactivar el DAC.
 *
 * Se usa un chirp en vez de un tono fijo porque su autocorrelación
 * tiene un pico mucho más angosto y dominante (menos lóbulos laterales
 * ambiguos), lo que reduce falsos positivos al buscar el eco cuando
 * la señal recibida es débil (objeto lejano).
 */
/**
 * Estructura de Filtro Pasa Banda Biquad IIR de 2do orden
 * Diseñado para atenuar ruido fuera de la banda del chirp (1 kHz - 3 kHz).
 */
struct BiquadBandpass {
    float b0, b2, a1, a2;
    float x1, x2, y1, y2;

    void init(float f_centro = 2000.0f, float ancho_banda = 2000.0f, float fs = 10000.0f);
    float process(float x);
    void reset();
};

/**
 * Filtro de Mediana Móvil para supresión de valores atípicos (outliers/multitrayectoria)
 */
struct FiltroMediana {
    static const int VENTANA_MAX = 7;
    float buffer[VENTANA_MAX];
    int tamano;
    int indice;
    int llenos;

    void init(int n_ventana = 5);
    void reset();
    float actualizar(float nueva_distancia);
};

/**
 * Sintetiza la señal de sondeo acústico s[n] como un CHIRP LINEAL
 * (barrido de frecuencia de f0 a f1) con modulación Hann para evitar
 * transitorios bruscos al activar/desactivar el DAC.
 */
void dsp_sintetizar_pulso(uint8_t* dac_out, float* ref_out, int n_puntos, float f0, float f1, float fs);

/**
 * Calcula la Transformada Rápida de Fourier (FFT) o su Inversa (IFFT)
 * utilizando el algoritmo Radix-2 Cooley-Tukey (in-place).
 */
void dsp_fft_radix2(Complex* X, int N, bool es_inversa = false);

/**
 * Calcula la correlación cruzada lineal en el dominio de la frecuencia:
 * R_rs[m] = IFFT( FFT(r) * conj(FFT(s)) )
 * con Zero-Padding a N_fft puntos.
 */
void dsp_correlacion_cruzada(
    const float* senal_rx, int n_rx,
    const float* senal_tx, int n_tx,
    float* r_out, int n_fft
);

/**
 * Identifica el pico principal de correlación fuera de la zona ciega
 * y calcula el tiempo de vuelo (tau) y la distancia al objeto.
 */
bool dsp_estimar_distancia(
    const float* R, int n_corr,
    int zona_ciega, float fs, float vel_sonido,
    float umbral_min,
    int& m_pico, float& tau_ms, float& dist_cm, float& amp_pico
);

#endif // DSP_RADAR_H
