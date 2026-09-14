import time
import math
import random
import matplotlib.pyplot as plt


#IMPLEMENTACIÓN DE DFT Y FFT
def dft(x):
    """
    Calcula la DFT mediante sumatoria directa con dos bucles for anidados.
    Complejidad computacional: O(N^2)
    """
    N = len(x) #Numero de muestras de la señal
    X = []  #Arreglo para almacenar los resultados de la DFT
    
    for k in range(N): #va probando cada frecuencia k hasta N-1
        suma = 0.0 + 0.0j #variable compleja para acumular la suma de la DFT
        for n in range(N): #recorre todo el audio en el tiempo, muestra por muestra
            angulo = -2.0 * math.pi * k * n / N #argumento de la exponencial de Fourier -2pi*k*n/N
            # Formula de Euler: e^(j*angulo) = cos(angulo) + j*sin(angulo)
            onda_prueba = complex(math.cos(angulo), math.sin(angulo))
            suma += x[n] * onda_prueba #producto punto 
        X.append(suma)#al recorrer todo el tiempo, se guarda el resultado total de esa frecuencia k y pasa a la siguiente.
        
    return X


def fft(x):
    """
    Calcula la FFT mediante el algoritmo Radix-2 de Cooley-Tukey (Divide y Venceras).
    Divide recursivamente las muestras pares e impares.
    Complejidad computacional: O(N log N)
    Requiere que N sea potencia de 2.
    """
    N = len(x)
    # Caso base de la recursion, cuando solo hay una muestra la frecuencia es la misma señal
    if N <= 1:
        return x
    
    #El truco del algoritmo,en vez de procesar N muestras, 
    #se separan en dos listas: una con las muestras pares y otra con las impares,
    #  y se llama recursivamente a la FFT para cada una de ellas.
    pares = fft(x[0::2])
    impares = fft(x[1::2])
    
    # Combinación (Mariposas / Twiddle factors):
    # W_N^k = e^(-j * 2 * pi * k / N)
    primera_mitad = []
    segunda_mitad = []
    for k in range(N // 2): #solo calcula la mitad de las frecuencias, la otra mitad es simétrica
        angulo = -2.0 * math.pi * k / N
        factor_giro = complex(math.cos(angulo), math.sin(angulo)) * impares[k]
        primera_mitad.append(pares[k] + factor_giro) #aca se ven las mariposas de fourier, como las ondas senoidales se invierten a los 180 grados
        segunda_mitad.append(pares[k] - factor_giro) #la segunda mitad del espectro es identica a la primera pero con el signo contrario,
                                                     #por eso solo necesitamos calcular la mitad de las frecuencias,
    return primera_mitad + segunda_mitad


#Prueba de validación de la DFT y FFT a ver si dan el mismo resultado
senal_test = [1.0, 2.0, -1.0, 3.0, 0.5, -2.0, 1.5, -0.5]
X_dft = dft(senal_test) #como podria llamar a esta variable mejor? 
X_fft = fft(senal_test)

#Mide la diferencia maxima punto a punto entre la DFT y la FFT
error_max = max(abs(d - f) for d, f in zip(X_dft, X_fft))
assert error_max < 1e-9, f"Error: La DFT difiere de la FFT por {error_max}"
print(f"[OK] DFT y FFT con éxito (Diferencia maxima = {error_max:.2e}).\n")


