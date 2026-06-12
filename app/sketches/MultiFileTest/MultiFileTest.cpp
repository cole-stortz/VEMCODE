// @board Arduino Uno
#include "blinker.h"

#define LED_PIN 13

BlinkPattern fast = {150, 150};
BlinkPattern slow = {800, 200};

bool ledState      = false;
unsigned long prev = 0;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(9600);
    Serial.println("MultiFile test ready — fast/slow blink driven by blinker.cpp");
}

void loop() {
    BlinkPattern& pat = ((millis() / 3000) % 2 == 0) ? fast : slow;
    int interval = next_interval(pat, ledState);

    if (millis() - prev >= (unsigned long)interval) {
        prev = millis();
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        Serial.print("LED ");
        Serial.println(ledState ? "ON" : "OFF");
    }
}