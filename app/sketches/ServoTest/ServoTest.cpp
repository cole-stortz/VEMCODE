#define SERVO_PIN 9

void setup() {
    Serial.begin(9600);
    pinMode(SERVO_PIN, OUTPUT);
}

void loop() {
    // Sweep 0 to 180
    for (int angle = 0; angle <= 180; angle += 10) {
        int pwm = angle * 255 / 180;
        analogWrite(SERVO_PIN, pwm);
        Serial.print("Angle: ");
        Serial.println(angle);
        delay(200);
    }

    // Sweep 180 to 0
    for (int angle = 180; angle >= 0; angle -= 10) {
        int pwm = angle * 255 / 180;
        analogWrite(SERVO_PIN, pwm);
        Serial.print("Angle: ");
        Serial.println(angle);
        delay(200);
    }
}
