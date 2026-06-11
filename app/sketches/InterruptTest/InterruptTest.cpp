// @board Arduino Uno
// Tests attachInterrupt, interrupts(), and noInterrupts().
// Note: VEMCODE registers the callback but does not fire it from pin changes --
// hardware interrupt simulation requires a CPU emulator. The test verifies that
// the calls compile and run without crashing.

#define BUTTON_PIN 2

volatile int isr_count = 0;

void onButtonFall() {
    isr_count++;
}

void setup() {
    Serial.begin(9600);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(BUTTON_PIN, onButtonFall, FALLING);
    Serial.println("=== InterruptTest ===");
    Serial.println("attachInterrupt registered (callback won't fire in simulation)");

    noInterrupts();
    Serial.println("interrupts disabled");
    interrupts();
    Serial.println("interrupts re-enabled");
}

void loop() {
    watch_variable("isr_count", isr_count);
    delay(500);
}