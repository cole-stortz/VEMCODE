// @board Arduino Uno
// Light-dependent resistor (LDR) -- an analog input.
#define LDR_PIN A1

void setup() {
    Serial.begin(9600);
}

void loop() {
    int ldr = analogRead(LDR_PIN);
    Serial.print("light: ");
    Serial.println(ldr);
    delay(200);
}
