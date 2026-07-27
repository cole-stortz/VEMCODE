// @board Arduino Uno
#include <Servo.h>

#define SERVO_PIN 9

Servo myServo;
int angle = 0;
int step  = 5;

void setup() {
    Serial.begin(9600);
    myServo.attach(SERVO_PIN);
}

void loop() {
    angle += step;
    if (angle >= 180 || angle <= 0) step = -step;

    myServo.write(angle);
    Serial.print("angle: ");
    Serial.println(angle);
    delay(50);
}
