// @board Arduino Uno
// Tests the Servo class itself (attach/write/read/attached/detach), not just
// raw analogWrite on a servo-ish pin -- OutputTest drives a servo pin directly
// and never exercises this class at all.
#include <Servo.h>

#define SERVO_PIN 9

Servo myServo;

void setup() {
    Serial.begin(9600);
    Serial.println("=== ServoTest ===");

    Serial.print("attached() before attach(): ");
    Serial.println(myServo.attached() ? "true" : "false"); // false

    myServo.attach(SERVO_PIN);
    Serial.print("attached() after attach(): ");
    Serial.println(myServo.attached() ? "true" : "false"); // true
}

void loop() {
    for (int angle = 0; angle <= 180; angle += 45) {
        myServo.write(angle);
        Serial.print("write(");
        Serial.print(angle);
        Serial.print(") -> read() = ");
        Serial.println(myServo.read());
        delay(200);
    }

    myServo.detach();
    Serial.print("after detach(), attached() = ");
    Serial.println(myServo.attached() ? "true" : "false"); // false

    myServo.attach(SERVO_PIN);
    delay(500);
}
