// @board Arduino Uno
// Tests attachInterrupt, ISR dispatch, interrupts()/noInterrupts().
//
// How to test:
//   Click the Button (pin 2) on the canvas -- each FALLING edge calls onButtonFall()
//   and increments isr_count. Watch it update in the Variable Watch panel and
//   in the serial monitor.

#define BUTTON_PIN 2

volatile int isr_count = 0;

void onButtonFall() {
    isr_count++;
}

void setup() {
    Serial.begin(9600);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // digitalPinToInterrupt() is the correct way -- defined as a no-op passthrough
    // in the injected header so both forms compile.
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonFall, FALLING);

    Serial.println("=== InterruptTest ===");
    Serial.println("Click the Button on the canvas to fire the ISR.");

    noInterrupts();
    Serial.println("interrupts disabled (noInterrupts)");
    interrupts();
    Serial.println("interrupts re-enabled (interrupts)");
}

static int last_count = -1;

void loop() {
    if (isr_count != last_count) {
        Serial.print("ISR fired! isr_count = ");
        Serial.println(isr_count);
        last_count = isr_count;
    }
    watch_variable("isr_count", isr_count);
    delay(50);
}
