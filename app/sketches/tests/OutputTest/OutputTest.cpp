// @board Teensy 4.1

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

#define STEP_PIN     20
#define DIR_PIN      21
#define PHASE_IN1  22
#define PHASE_IN2  23
#define PHASE_IN3  24
#define PHASE_IN4  25
#define SEG_A        26
#define SEG_B        27
#define SEG_C        28
#define SEG_D        29
#define SEG_E        30
#define SEG_F        31
#define SEG_G        32

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
int loopCount = 0;

int phaseSeq[4][4] = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1},
};

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
    pinMode(STEP_PIN,   OUTPUT);
    pinMode(DIR_PIN,    OUTPUT);
    pinMode(PHASE_IN1, OUTPUT);
    pinMode(PHASE_IN2, OUTPUT);
    pinMode(PHASE_IN3, OUTPUT);
    pinMode(PHASE_IN4, OUTPUT);
    pinMode(SEG_A, OUTPUT);
    pinMode(SEG_B, OUTPUT);
    pinMode(SEG_C, OUTPUT);
    pinMode(SEG_D, OUTPUT);
    pinMode(SEG_E, OUTPUT);
    pinMode(SEG_F, OUTPUT);
    pinMode(SEG_G, OUTPUT);
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("VEMCODE Output");
    Serial.println("OutputTest started");
}

void stepPulse() {
    digitalWrite(STEP_PIN, HIGH);
    delay(20);
    digitalWrite(STEP_PIN, LOW);
    delay(20);
}

void setPhase(int idx) {
    digitalWrite(PHASE_IN1, phaseSeq[idx][0]);
    digitalWrite(PHASE_IN2, phaseSeq[idx][1]);
    digitalWrite(PHASE_IN3, phaseSeq[idx][2]);
    digitalWrite(PHASE_IN4, phaseSeq[idx][3]);
}

void showDigit(bool a, bool b, bool c, bool d, bool e, bool f, bool g) {
    digitalWrite(SEG_A, a);
    digitalWrite(SEG_B, b);
    digitalWrite(SEG_C, c);
    digitalWrite(SEG_D, d);
    digitalWrite(SEG_E, e);
    digitalWrite(SEG_F, f);
    digitalWrite(SEG_G, g);
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

    // Stepper (STEP/DIR style)
    digitalWrite(DIR_PIN, HIGH); // CW
    for (int i = 0; i < 5; i++) stepPulse();
    Serial.println("Stepper: 5 CW steps");
    delay(300);
    digitalWrite(DIR_PIN, LOW); // CCW
    for (int i = 0; i < 3; i++) stepPulse();
    Serial.println("Stepper: 3 CCW steps");
    delay(300);

    // Stepper (IN1-IN4 phase style)
    for (int i = 0; i < 4; i++) { setPhase(i); delay(50); }
    Serial.println("Stepper phase: 4 CW steps");
    delay(300);
    for (int i = 3; i >= 0; i--) { setPhase(i); delay(50); }
    Serial.println("Stepper phase: 4 CCW steps");
    delay(300);

    // Seven-segment display
    showDigit(1, 1, 1, 0, 0, 0, 0); // "7"
    Serial.println("SevenSeg: 7");
    delay(500);
    showDigit(1, 1, 1, 1, 1, 1, 1); // "8"
    Serial.println("SevenSeg: 8");
    delay(500);
    showDigit(0, 0, 0, 0, 0, 0, 0); // blank
    Serial.println("SevenSeg: blank");
    delay(500);
}
