// @board Arduino Uno
// Bouncy button -- click it on the canvas and watch it settle after the
// ~10ms bounce window before reporting a clean state.
#define BUTTON_PIN 2

void setup() {
    Serial.begin(9600);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
    bool pressed = digitalRead(BUTTON_PIN) == LOW;
    Serial.print("button: ");
    Serial.println(pressed ? "pressed" : "released");
    delay(100);
}
