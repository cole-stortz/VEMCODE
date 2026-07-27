// @board Arduino Uno
// Potentiometer -- drag it on the canvas and watch the analog reading change.
#define POT_PIN A0

void setup() {
    Serial.begin(9600);
}

void loop() {
    int pot = analogRead(POT_PIN);
    Serial.print("pot: ");
    Serial.println(pot);
    delay(200);
}
