// @board Arduino Uno
// Toggle switch -- click it on the canvas to flip its state (stays flipped,
// unlike a momentary button).
#define SWITCH_PIN 3

void setup() {
    Serial.begin(9600);
    pinMode(SWITCH_PIN, INPUT);
}

void loop() {
    bool on = digitalRead(SWITCH_PIN);
    Serial.print("switch: ");
    Serial.println(on ? "ON" : "OFF");
    delay(200);
}
