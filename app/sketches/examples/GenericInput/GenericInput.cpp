// @board Arduino Uno
// A pin whose name doesn't match any known component keyword falls back to a
// plain Generic Input on the canvas -- still fully readable/writable, just
// with no special visualization.
#define MYSTERY_PIN 25

void setup() {
    Serial.begin(9600);
    pinMode(MYSTERY_PIN, INPUT);
}

void loop() {
    Serial.print("mystery pin: ");
    Serial.println(digitalRead(MYSTERY_PIN));
    delay(200);
}
