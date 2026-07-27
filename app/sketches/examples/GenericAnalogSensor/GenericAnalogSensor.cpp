// @board Arduino Uno
// A pin named with a generic sensor keyword (SENSOR/ANALOG/ADC) that doesn't
// match a more specific sensor type still gets a usable analog input on the
// canvas.
#define ADC_PIN A0

void setup() {
    Serial.begin(9600);
}

void loop() {
    int value = analogRead(ADC_PIN);
    Serial.print("adc: ");
    Serial.println(value);
    delay(200);
}
