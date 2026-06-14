// @board Arduino Uno
// Intentionally triggers all of VEMCODE's static analysis warnings.
// The sketch compiles and runs -- warnings appear in the serial monitor BEFORE
// the compile output.
//
// Expected warnings (one per check):
//
//  1. WARNING: Pin 20 ('SENSOR_PIN') is not available on the Arduino Uno (max pin 19)
//
//  2. WARNING: Pin 4 does not support PWM on the Arduino Uno -- analogWrite() will have no effect
//
//  3. WARNING: attachInterrupt(0, ...) uses an interrupt number, not a pin number
//             -- use digitalPinToInterrupt(2) to attach to pin 2 on the Arduino Uno
//
//  4. WARNING: delay() inside 'onEdge' (used as an interrupt handler) will hang on real Arduino
//
//  5. WARNING: Pin 'COMPUTED_PIN' is defined as an expression -- the simulator could not
//             evaluate it and the component may not appear on the canvas

#define LED_PIN      13
#define BUZZ_PIN     4    // pin 4 is NOT a PWM pin on Uno → warning #2
#define SENSOR_PIN   20   // Uno has pins 0-19; 20 is out of range → warning #1
#define COMPUTED_PIN (LED_PIN + 0)  // expression → warning #5

volatile int edge_count = 0;

void onEdge() {
    edge_count++;
    delay(5);  // delay() inside an ISR callback → warning #4
}

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);

    // raw interrupt number 0 on Uno (should use digitalPinToInterrupt(2)) → warning #3
    attachInterrupt(0, onEdge, RISING);

    Serial.println("=== StaticAnalysisTest ===");
    Serial.println("Scroll up to see the 5 warnings above the compile output.");
    Serial.println("The sketch runs normally -- warnings are informational.");
}

void loop() {
    static int last = 0;
    if (edge_count != last) {
        Serial.print("onEdge fired: ");
        Serial.println(edge_count);
        last = edge_count;
    }

    // analogWrite on non-PWM pin → warning #2 (fires every loop to keep it testable)
    analogWrite(BUZZ_PIN, 128);

    watch_variable("edge_count", edge_count);
    delay(500);
}
