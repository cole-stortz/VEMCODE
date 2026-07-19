// @board Arduino Uno
// Bare ENA/IN1/IN2 naming (no shared prefix) -- L298N/L293D single-motor
// style. Should detect as one HBridgeMotor, not three separate pins.
#define ENA 5
#define IN1 6
#define IN2 7

void setup() {
    pinMode(ENA, OUTPUT);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, 200);
}

void loop() {
    delay(1000);
}
