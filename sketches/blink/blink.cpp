
#include "src/core/runtime/arduinoapi.h"
#include <string>

// Pull Arduino constants into global scope for this sketch.
// This means user code can write HIGH, LOW, OUTPUT exactly like real Arduino.
using namespace vb;

#define LED_PIN 12

// The API pointer injected by the host
static ArduinoAPI* api = nullptr;

// Sketch state
static int counter = 0;

// Called once after LoadLibrary -- store the API pointer
extern "C" __declspec(dllexport)
void vb_init(ArduinoAPI* injected_api) {
    api = injected_api;
}

// Maps to Arduino setup()
extern "C" __declspec(dllexport)
void vb_setup() {
    api->Serial_begin(9600);
    api->pinMode(LED_PIN, OUTPUT);
    api->Serial_println("=== Sketch started ===");
}

// Maps to Arduino loop()
extern "C" __declspec(dllexport)
void vb_loop() {
    api->digitalWrite(LED_PIN, HIGH);
    api->delay(100);
    api->digitalWrite(LED_PIN, LOW);
    api->delay(100);

    counter++;
    std::string msg = "Blink #" + std::to_string(counter) + " - pin 13 toggled";
    api->Serial_println(msg.c_str());
}