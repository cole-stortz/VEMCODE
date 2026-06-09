// @board Arduino Uno

#define LED_PIN      13
#define BUZZER_PIN   11
#define SERVO_PIN     9
#define MOTOR_PWM     5
#define MOTOR_CW      6
#define MOTOR_ANTI    7

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN,    OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SERVO_PIN,  OUTPUT);
    pinMode(MOTOR_PWM,  OUTPUT);
    pinMode(MOTOR_CW,   OUTPUT);
    pinMode(MOTOR_ANTI, OUTPUT);
    Serial.println("OutputTest started");
}

void loop() {
    // LED
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED ON");
    delay(500);
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED OFF");

    // Buzzer
    //tone(BUZZER_PIN, 440);
    //Serial.println("Buzzer ON");
    //delay(500);
    //noTone(BUZZER_PIN);
    //Serial.println("Buzzer OFF");

    // Servo sweep 0 -> 180
    Serial.println("Servo sweep");
    for (int angle = 0; angle <= 180; angle += 30) {
        analogWrite(SERVO_PIN, angle * 255 / 180);
        Serial.print("  angle: ");
        Serial.println(angle);
        delay(200);
    }

    // Motor forward then stop
    digitalWrite(MOTOR_CW,   HIGH);
    digitalWrite(MOTOR_ANTI, LOW);
    analogWrite(MOTOR_PWM,   200);
    Serial.println("Motor forward");
    delay(500);
    analogWrite(MOTOR_PWM, 0);
    digitalWrite(MOTOR_CW, LOW);
    Serial.println("Motor stop");
    delay(500);
}
