import time
import math
import random
import matplotlib.pyplot as plt

#============================
#IMPLEMENTACIÓN DE DFT Y FFT
#============================
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


# ===================================
# COMPARACION DE TIEMPOS DE EJECUCION 
# ===================================

# Tamaños de prueba, el algoritmo de FFT requiere que N sea potencia de 2
valores_N = [32, 64, 128, 256, 512, 1024]
#listas para guardar los tiempos de ejecución de cada algoritmo
tiempos_dft = [] 
tiempos_fft = []

print(f"{'N':<8}{'Tiempo DFT (ms)':<20}{'Tiempo FFT (ms)':<20}{'Aceleración':<15}")
print("-" * 60)

for N in valores_N:
    #Genera una señal aleatoria de tamaño N con distribución gaussiana (media=0, desviación=1)
    x = [random.gauss(0.0, 1.0) for _ in range(N)]
    
    # 1.Se toma el tiempo de la DFT (O(N^2))
    t0 = time.perf_counter()
    _ = dft(x)
    t_dft = (time.perf_counter() - t0) * 1000.0  # en milisegundos
    tiempos_dft.append(t_dft)
    
    # 2.Se toma el tiempo de la FFT(O(N log N))
    repeticiones = 20 #bucle de 20 repeticiones porque el algoritmo de FFT es muy rápido y necesitamos un tiempo medible.
    t0 = time.perf_counter()
    for _ in range(repeticiones):
        _ = fft(x)
    t_fft = ((time.perf_counter() - t0) / repeticiones) * 1000.0 #asi que se promedia ejecutando 20 veces y dividiendo el tiempo total entre 20.
    tiempos_fft.append(t_fft)
    
    aceleracion = t_dft / t_fft if t_fft > 0 else 0 #factor de Speedup, cuantas veces es mas rapida la fft vs la dft
    print(f"{N:<8}{t_dft:<20.4f}{t_fft:<20.4f}{aceleracion:<15.1f}x")

#Graficas comparativas de rendimiento
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))

# Escala lineal
ax1.plot(valores_N, tiempos_dft, 'o-', color='tab:red', label=r'DFT Manual ($O(N^2)$)')
ax1.plot(valores_N, tiempos_fft, 's-', color='tab:blue', label=r'FFT Radix-2 ($O(N \log N)$)')
ax1.set_title('Comparativa de Rendimiento (Escala Lineal)')
ax1.set_xlabel('Tamaño de muestra (N)')
ax1.set_ylabel('Tiempo de ejecución (ms)')
ax1.grid(True, linestyle='--', alpha=0.6)
ax1.legend()

# Escala semilogarítmica
ax2.plot(valores_N, tiempos_dft, 'o-', color='tab:red', label=r'DFT Manual ($O(N^2)$)')
ax2.plot(valores_N, tiempos_fft, 's-', color='tab:blue', label=r'FFT Radix-2 ($O(N \log N)$)')
ax2.set_yscale('log')
ax2.set_title('Comparativa de Rendimiento (Escala Semilogarítmica)')
ax2.set_xlabel('Tamaño de muestra (N)')
ax2.set_ylabel('Tiempo de ejecución (ms) [Log]')
ax2.grid(True, which="both", linestyle='--', alpha=0.6)
ax2.legend()

plt.tight_layout()
plt.savefig('figura_comparativa_tiempos.png', dpi=300)
print("\n[OK] Bloque 2: Gráfica 'figura_comparativa_tiempos.png' guardada.")


