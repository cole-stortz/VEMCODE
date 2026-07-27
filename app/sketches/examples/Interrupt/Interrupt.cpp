// @board Arduino Uno
// attachInterrupt, ISR dispatch, interrupts()/noInterrupts().
// Click the Button (pin 2) on the canvas -- each FALLING edge calls
// onButtonFall() and increments isr_count. Watch it update live in the
// Variable Watch panel and in the serial monitor.
#define BUTTON_PIN 2

volatile int isr_count = 0;

void onButtonFall() {
    isr_count++;
}

void setup() {
    Serial.begin(9600);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // digitalPinToInterrupt() is the correct way to write this on real hardware.
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonFall, FALLING);

    Serial.println("=== Interrupt ===");
    Serial.println("Click the Button on the canvas to fire the ISR.");
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
