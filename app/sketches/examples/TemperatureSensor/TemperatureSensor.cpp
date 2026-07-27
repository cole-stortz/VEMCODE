// @board Arduino Uno
// Analog temperature sensor (e.g. a thermistor or TMP36) -- an analog input.
#define TEMP_PIN A2

void setup() {
    Serial.begin(9600);
}

void loop() {
    int temp = analogRead(TEMP_PIN);
    Serial.print("temp: ");
    Serial.println(temp);
    delay(200);
}
