// @board Arduino Uno
// Stepper motor, STEP/DIR driver style (e.g. A4988/DRV8825).
#define STEP_PIN 8
#define DIR_PIN  9

void setup() {
    Serial.begin(9600);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN,  OUTPUT);
}

void pulse() {
    digitalWrite(STEP_PIN, HIGH);
    delay(20);
    digitalWrite(STEP_PIN, LOW);
    delay(20);
}

void loop() {
    digitalWrite(DIR_PIN, HIGH); // CW
    for (int i = 0; i < 5; i++) pulse();
    Serial.println("5 CW steps");
    delay(300);

    digitalWrite(DIR_PIN, LOW); // CCW
    for (int i = 0; i < 3; i++) pulse();
    Serial.println("3 CCW steps");
    delay(300);
}
