// @board Arduino Uno
// Space-before-paren API calls should still get rewritten to api-> calls.
void setup() {
    Serial.begin (9600);
    pinMode (13, OUTPUT);
}

void loop() {
    digitalWrite (13, HIGH);
    delay (200);
    digitalWrite (13, LOW);
    delay (200);
}
