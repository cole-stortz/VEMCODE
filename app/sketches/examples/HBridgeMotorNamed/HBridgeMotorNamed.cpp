// @board Arduino Uno
// H-Bridge motor with a shared MOTOR_ prefix (Prefix detection strategy) --
// an alternative naming style to the bare ENA/IN1/IN2 form.
#define MOTOR_PWM  5
#define MOTOR_CW   6
#define MOTOR_ANTI 7

void setup() {
    Serial.begin(9600);
    pinMode(MOTOR_PWM,  OUTPUT);
    pinMode(MOTOR_CW,   OUTPUT);
    pinMode(MOTOR_ANTI, OUTPUT);
}

void loop() {
    digitalWrite(MOTOR_CW,   HIGH);
    digitalWrite(MOTOR_ANTI, LOW);
    analogWrite(MOTOR_PWM,   200);
    Serial.println("Motor forward");
    delay(1000);

    analogWrite(MOTOR_PWM, 0);
    digitalWrite(MOTOR_CW, LOW);
    Serial.println("Motor stop");
    delay(1000);
}
