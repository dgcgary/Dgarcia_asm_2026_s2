/**
 * ============================================================================
 * TEST 01: BLINK LED (Verificación básica de carga y compilación)
 * ============================================================================
 * Placa: ESP32-WROOM-32D
 * Pin: GPIO 2 (LED azul integrado)
 */

#include <Arduino.h>

#define LED_PIN 2

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(LED_PIN, OUTPUT);
    Serial.println("=== TEST 01: ESP32 Conectado y Funcionando ===");
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED Encendido");
    delay(500);

    digitalWrite(LED_PIN, LOW);
    Serial.println("LED Apagado");
    delay(500);
}
