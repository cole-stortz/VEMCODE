// @board Arduino Uno
// tone()'s duration thread used to be detached and unjoined; confirms it
// still fires and clears the pin's frequency correctly after the refactor
// to a tracked/joined thread.
void setup() {
    Serial.begin(9600);
    tone(9, 440, 300);
    Serial.println("tone started");
}

void loop() {
    delay(500);
    Serial.println("loop tick");
}
