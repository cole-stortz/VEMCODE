// @board Arduino Uno
// tone()/noTone() -- watch the buzzer component light up active on the
// canvas while the tone plays.
void setup() {
    Serial.begin(9600);
    tone(9, 440, 300);
    Serial.println("tone started");
}

void loop() {
    delay(500);
    Serial.println("loop tick");
}
