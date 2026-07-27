// @board Arduino Uno
// Clean (ideal) button -- named with CLEAN/IDEAL so it's exempt from the
// ~10ms bounce simulation a regular Button gets.
#define CLEAN_PIN 8

void setup() {
    Serial.begin(9600);
    pinMode(CLEAN_PIN, INPUT_PULLUP);
}

void loop() {
    bool pressed = digitalRead(CLEAN_PIN) == LOW;
    Serial.print("clean button: ");
    Serial.println(pressed ? "pressed" : "released");
    delay(100);
}
