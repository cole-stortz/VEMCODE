#include "src/core/circuit/circuitdetector.h"
#include <iostream>

int main() {
    std::string sketch = R"(
        #define LED_PIN 13
        #define BUTTON_PIN 2
        #define BUZZER_PIN 8

        void setup() {
            pinMode(LED_PIN, OUTPUT);
            pinMode(BUTTON_PIN, INPUT_PULLUP);
            pinMode(BUZZER_PIN, OUTPUT);
            Serial.begin(9600);
        }

        void loop() {
            digitalWrite(LED_PIN, HIGH);
        }
    )";

    CircuitDetector detector;
    detector.detect(sketch);

    std::cout << "Detected " << detector.components().size() << " components:\n";
    for (const auto& c : detector.components()) {
        std::cout << "  " << c.label << "\n";
    }
}