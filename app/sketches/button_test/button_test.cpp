#include "src/core/runtime/arduinoapi.h"
#include <string>
using namespace vb;

#define LED_PIN    13
#define BUTTON_PIN 2

int btn = 1;

static ArduinoAPI* api = nullptr;

extern "C" __declspec(dllexport)
void vb_init(ArduinoAPI* a) { api = a; }

extern "C" __declspec(dllexport)
void vb_setup() {
    api->Serial_begin(9600);
    api->pinMode(LED_PIN, OUTPUT);
    api->pinMode(BUTTON_PIN, INPUT_PULLUP);

    api->digitalWrite(BUTTON_PIN, LOW);
    api->digitalWrite(LED_PIN, LOW);
    
    api->Serial_println("Ready — click the button on the canvas");
}

extern "C" __declspec(dllexport)
void vb_loop() {
    
    btn = api->digitalRead(BUTTON_PIN);
    if (btn == 0) {
        api->digitalWrite(LED_PIN, HIGH);
        api->Serial_println("Button pressed — LED ON");
    } else {
        api->digitalWrite(LED_PIN, LOW);
    }
    api->delay(50);
}