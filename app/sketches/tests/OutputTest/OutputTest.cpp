// @board Arduino Uno

#include <LiquidCrystal.h>

#define LED_PIN      13
#define BUZZER_PIN   11
#define SERVO_PIN     9
#define MOTOR_PWM     5
#define MOTOR_CW      6
#define MOTOR_ANTI    7
#define RED_PIN       3
#define GREEN_PIN     8
#define BLUE_PIN     10
#define LCD_RS        2
#define LCD_EN        4
#define LCD_D4       12
#define LCD_D5       A0
#define LCD_D6       A1
#define LCD_D7       A2
#define MYSTERY_OUTPUT_PIN A3

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
int loopCount = 0;

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN,    OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SERVO_PIN,  OUTPUT);
    pinMode(MOTOR_PWM,  OUTPUT);
    pinMode(MOTOR_CW,   OUTPUT);
    pinMode(MOTOR_ANTI, OUTPUT);
    pinMode(RED_PIN,    OUTPUT);
    pinMode(GREEN_PIN,  OUTPUT);
    pinMode(BLUE_PIN,   OUTPUT);
    pinMode(MYSTERY_OUTPUT_PIN, OUTPUT);
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("VEMCODE Output");
    Serial.println("OutputTest started");
}

void loop() {
    loopCount++;
    lcd.setCursor(0, 1);
    lcd.print("Loop: ");
    lcd.print(loopCount);
    lcd.print("   ");

    // LED
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED ON");
    delay(500);
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED OFF");

    // Buzzer
    tone(BUZZER_PIN, 440);
    Serial.println("Buzzer ON");
    delay(500);
    noTone(BUZZER_PIN);
    Serial.println("Buzzer OFF");

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

    // RGB LED color cycle
    analogWrite(RED_PIN, 255); analogWrite(GREEN_PIN, 0);   analogWrite(BLUE_PIN, 0);
    Serial.println("RGB: red");
    delay(500);
    analogWrite(RED_PIN, 0);   analogWrite(GREEN_PIN, 255); analogWrite(BLUE_PIN, 0);
    Serial.println("RGB: green");
    delay(500);
    analogWrite(RED_PIN, 0);   analogWrite(GREEN_PIN, 0);   analogWrite(BLUE_PIN, 255);
    Serial.println("RGB: blue");
    delay(500);
    analogWrite(RED_PIN, 0);   analogWrite(GREEN_PIN, 0);   analogWrite(BLUE_PIN, 0);
    Serial.println("RGB: off");
    delay(500);
}
