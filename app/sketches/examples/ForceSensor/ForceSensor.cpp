// @board Arduino Uno
// Force-sensitive resistor (FSR) -- an analog input.
#define FORCE_PIN A5

void setup() {
    Serial.begin(9600);
}

void loop() {
    int force = analogRead(FORCE_PIN);
    Serial.print("force: ");
    Serial.println(force);
    delay(200);
}
