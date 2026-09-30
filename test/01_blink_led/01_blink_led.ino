#include <Arduino.h>

#define LED_PIN 2

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(LED_PIN, OUTPUT);
    Serial.println("Test 01: ESP32 conectado y funcionando");
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED encendido");
    delay(500);

    digitalWrite(LED_PIN, LOW);
    Serial.println("LED apagado");
    delay(500);
}
