// -------------------------------------------------------
// Example sketch -- equivalent to a basic Arduino sketch:
//
//   int counter = 0;
//   void setup() {
//       Serial.begin(9600);
//       pinMode(13, OUTPUT);
//   }
//   void loop() {
//       digitalWrite(13, HIGH);
//       delay(500);
//       digitalWrite(13, LOW);
//       delay(500);
//       counter++;
//       Serial.print("Loop count: ");
//       Serial.println(std::to_string(counter).c_str());
//   }
// -------------------------------------------------------

#include "src/core/runtime/arduinoapi.h"
#include <string>

// Pull Arduino constants into global scope for this sketch.
// This means user code can write HIGH, LOW, OUTPUT exactly like real Arduino.
using namespace vb;

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
    api->pinMode(13, OUTPUT);
    api->Serial_println("=== Sketch started ===");
}

// Maps to Arduino loop()
extern "C" __declspec(dllexport)
void vb_loop() {
    api->digitalWrite(13, HIGH);
    api->delay(100);
    api->digitalWrite(13, LOW);
    api->delay(100);

    counter++;
    std::string msg = "Blink #" + std::to_string(counter) + " - pin 13 toggled";
    api->Serial_println(msg.c_str());
}