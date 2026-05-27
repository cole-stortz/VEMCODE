#define ANALOGIN A0

int value = 0;

void setup() {
    Serial.begin(9600);
}

void loop() {
    value = analogRead(ANALOGIN);
    Serial.println(value);
    delay(100);
}
