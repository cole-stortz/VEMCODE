#include "src/core/runtime/arduinoapi.h"
#include <string>
#include <cstring>
using namespace vb;

static ArduinoAPI* api = nullptr;

extern "C" __declspec(dllexport)
void vb_init(ArduinoAPI* a) { api = a; }

#define LED_PIN 9

extern "C" __declspec(dllexport)
void vb_setup() {
    api->Serial_begin(9600);
    api->pinMode(LED_PIN, OUTPUT);
    api->Serial_println("VirtualBench ready");
}

extern "C" __declspec(dllexport)
void vb_loop() {
    api->digitalWrite(LED_PIN, HIGH);
    api->delay(1000);
    api->digitalWrite(LED_PIN, LOW);
    api->delay(1000);
    api->Serial_println("Blink");
}
