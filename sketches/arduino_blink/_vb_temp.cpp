#include "src/core/runtime/arduinoapi.h"
#include <string>
#include <cstring>
using namespace vb;

static ArduinoAPI* api = nullptr;

extern "C" __declspec(dllexport)
void vb_init(ArduinoAPI* a) { api = a; }

#DEFINE LED_PIN 13

extern "C" __declspec(dllexport)
void vb_setup() {
    api->Serial_begin(9600);
    api->pinMode(LED_PIN, OUTPUT);
    api->Serial_println("Starting blink");
}

extern "C" __declspec(dllexport)
void vb_loop() {
    api->digitalWrite(LED_PIN, HIGH);
    api->delay(100);
    api->digitalWrite(LED_PIN, LOW);
    api->delay(100);
    api->Serial_println("Blink");
}