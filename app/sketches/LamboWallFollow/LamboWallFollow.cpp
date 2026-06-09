/*
  VEMCODE adaptation of Lambo_LineFollow.ino
  Changes from original:
    - Pins remapped to 0-19 range (Teensy 4.1 pins went up to 41)
    - 4 color sensors reduced to 1 (sensor index 2 from original: pins 14-18 fit within 0-19)
    - Servo class replaced with analogWrite (Arm/Gripper not yet in VEMCODE runtime)
    - pulseIn calls updated to 3-arg form (function pointer has no default timeout)
  Algorithm logic is unchanged.
*/

// @board Arduino Uno

// Motor A (left)
#define MOTOR1_PWM           6
#define MOTOR1_CWISE         7
#define MOTOR1_ANTI_CWISE    8

// Motor B (center omni / strafe)
#define MOTOR2_PWM           0
#define MOTOR2_CWISE         1
#define MOTOR2_ANTI_CWISE    2

// Motor C (right)
#define MOTOR3_PWM           3
#define MOTOR3_CWISE         4
#define MOTOR3_ANTI_CWISE    5

#define PWM_VAL        100
#define PWM_VAL_MOTOR3 100
#define PWM_VAL_MOTOR2 100

// Color constants
#define BLACK  5
#define WHITE  1
#define YELLOW 6
#define GREEN  4
#define BLUE   3
#define RED    2

// Single TCS3200 color sensor — pins from original sensor index 2 (all in range 14-18)
#define NUM_SENSORS 1
const int S0[NUM_SENSORS]        = {A0};
const int S1[NUM_SENSORS]        = {A1};
const int S2[NUM_SENSORS]        = {A2};
const int S3[NUM_SENSORS]        = {A3};
const int sensorOut[NUM_SENSORS] = {A4};

// Calibration from original sensor index 2
int redMin[NUM_SENSORS]   = {41};
int redMax[NUM_SENSORS]   = {331};
int greenMin[NUM_SENSORS] = {42};
int greenMax[NUM_SENSORS] = {343};
int blueMin[NUM_SENSORS]  = {46};
int blueMax[NUM_SENSORS]  = {372};

// Ultrasonic sensor
#define ECHO_PIN 12
#define TRIG_PIN 11
long duration;
int distance;

// Algorithm variables
int x_pos       = 0;
int x_floating  = 35;
int delayCondition = 0;
int move_val    = 0;
int y_pos       = 0;
int colorsensorValues[NUM_SENSORS] = {0};

// ---- MOTOR FUNCTIONS ----

void stopMotors() {
    analogWrite(MOTOR2_PWM, PWM_VAL);
    digitalWrite(MOTOR1_CWISE, LOW);
    digitalWrite(MOTOR1_ANTI_CWISE, LOW);
    digitalWrite(MOTOR2_CWISE, LOW);
    digitalWrite(MOTOR2_ANTI_CWISE, LOW);
    digitalWrite(MOTOR3_CWISE, LOW);
    digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsForward() {
    Serial.println("Forward");
    analogWrite(MOTOR1_PWM, PWM_VAL);
    analogWrite(MOTOR2_PWM, PWM_VAL);
    analogWrite(MOTOR3_PWM, PWM_VAL);
    digitalWrite(MOTOR1_CWISE, LOW);
    digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
    digitalWrite(MOTOR2_CWISE, LOW);
    digitalWrite(MOTOR2_ANTI_CWISE, LOW);
    digitalWrite(MOTOR3_CWISE, HIGH);
    digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsBackward() {
    Serial.println("Backward");
    analogWrite(MOTOR1_PWM, PWM_VAL);
    analogWrite(MOTOR2_PWM, PWM_VAL);
    analogWrite(MOTOR3_PWM, PWM_VAL + 12);
    digitalWrite(MOTOR1_CWISE, HIGH);
    digitalWrite(MOTOR1_ANTI_CWISE, LOW);
    digitalWrite(MOTOR2_CWISE, LOW);
    digitalWrite(MOTOR2_ANTI_CWISE, LOW);
    digitalWrite(MOTOR3_CWISE, LOW);
    digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
}

void moveMotorsRight() {
    Serial.println("Right");
    analogWrite(MOTOR1_PWM, PWM_VAL);
    analogWrite(MOTOR2_PWM, (int)(PWM_VAL_MOTOR2 * 1.9));
    analogWrite(MOTOR3_PWM, PWM_VAL);
    digitalWrite(MOTOR1_CWISE, HIGH);
    digitalWrite(MOTOR1_ANTI_CWISE, LOW);
    digitalWrite(MOTOR2_CWISE, LOW);
    digitalWrite(MOTOR2_ANTI_CWISE, HIGH);
    digitalWrite(MOTOR3_CWISE, HIGH);
    digitalWrite(MOTOR3_ANTI_CWISE, LOW);
}

void moveMotorsLeft() {
    Serial.println("Left");
    analogWrite(MOTOR1_PWM, PWM_VAL);
    analogWrite(MOTOR2_PWM, (int)(PWM_VAL_MOTOR2 * 1.9));
    analogWrite(MOTOR3_PWM, PWM_VAL);
    digitalWrite(MOTOR1_CWISE, LOW);
    digitalWrite(MOTOR1_ANTI_CWISE, HIGH);
    digitalWrite(MOTOR2_CWISE, HIGH);
    digitalWrite(MOTOR2_ANTI_CWISE, LOW);
    digitalWrite(MOTOR3_CWISE, LOW);
    digitalWrite(MOTOR3_ANTI_CWISE, HIGH);
}

// ---- ULTRASONIC ----

void ultrasonicRead() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(10);
    duration = pulseIn(ECHO_PIN, HIGH, 1000000);
    distance = duration * 0.034 / 2;
    Serial.print(distance);
    Serial.println("cm");
}

// ---- COLOR SENSOR ----

unsigned long readColorFrequency(int sensorIndex, int s2, int s3) {
    digitalWrite(S2[sensorIndex], s2);
    digitalWrite(S3[sensorIndex], s3);
    return pulseIn(sensorOut[sensorIndex], LOW, 1000000);
}

int mapValue(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void readColorSensors() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        unsigned long redFrequency   = readColorFrequency(i, LOW, LOW);
        unsigned long blueFrequency  = readColorFrequency(i, LOW, HIGH);
        unsigned long greenFrequency = readColorFrequency(i, HIGH, HIGH);

        int mappedRed   = mapValue(redFrequency,   redMin[i],   redMax[i],   0, 255);
        int mappedGreen = mapValue(greenFrequency, greenMin[i], greenMax[i], 0, 255);
        int mappedBlue  = mapValue(blueFrequency,  blueMin[i],  blueMax[i],  0, 255);

        Serial.print("Sensor ");
        Serial.print(i + 1);
        Serial.print(" - Red: ");
        Serial.print(mappedRed);
        Serial.print(" | Green: ");
        Serial.print(mappedGreen);
        Serial.print(" | Blue: ");
        Serial.println(mappedBlue);

        if (mappedRed < 65 && mappedGreen < 65 && mappedBlue < 65) {
            colorsensorValues[i] = YELLOW;
            Serial.println("Color Est: Yellow");
        } else if (mappedRed > 100 && mappedGreen > 160 && mappedBlue > 100) {
            colorsensorValues[i] = BLACK;
            Serial.println("Color Est: Black");
        } else if (mappedRed < mappedBlue && mappedRed < mappedGreen) {
            colorsensorValues[i] = RED;
            Serial.println("Color Est: Red");
        } else if (mappedBlue < mappedRed && mappedBlue < mappedGreen) {
            colorsensorValues[i] = BLUE;
            Serial.println("Color Est: Blue");
        } else if (mappedGreen < mappedRed && mappedGreen < mappedBlue) {
            colorsensorValues[i] = GREEN;
            Serial.println("Color Est: Green");
        } else {
            Serial.println("Color not recognized");
            colorsensorValues[i] = 0;
        }
    }
}

// ---- SETUP / LOOP ----

void setup() {
    Serial.begin(57600);

    pinMode(MOTOR1_PWM, OUTPUT);
    pinMode(MOTOR2_PWM, OUTPUT);
    pinMode(MOTOR3_PWM, OUTPUT);
    pinMode(MOTOR1_CWISE, OUTPUT);
    pinMode(MOTOR1_ANTI_CWISE, OUTPUT);
    pinMode(MOTOR2_CWISE, OUTPUT);
    pinMode(MOTOR2_ANTI_CWISE, OUTPUT);
    pinMode(MOTOR3_CWISE, OUTPUT);
    pinMode(MOTOR3_ANTI_CWISE, OUTPUT);

    pinMode(ECHO_PIN, INPUT);
    pinMode(TRIG_PIN, OUTPUT);

    stopMotors();

    for (int i = 0; i < NUM_SENSORS; i++) {
        pinMode(S0[i], OUTPUT);
        pinMode(S1[i], OUTPUT);
        pinMode(S2[i], OUTPUT);
        pinMode(S3[i], OUTPUT);
        pinMode(sensorOut[i], INPUT);
        digitalWrite(S0[i], HIGH);
        digitalWrite(S1[i], LOW);
    }
}

void loop() {
    Serial.println("------------------");
    Serial.print("x position: ");
    Serial.println(x_floating);
    Serial.print("y position: ");
    Serial.println(y_pos);
    readColorSensors();
    ultrasonicRead();

    //////////////////////// OBJECT AVOIDANCE ALGORITHM /////////////////////////////
    if (distance > 20) {
        if (delayCondition == 1) {
            delay(500);
            if (move_val == 1) {
                x_floating = x_floating + 5;
            } else if (move_val == -1) {
                x_floating = x_floating - 5;
            }
            delayCondition = 0;
        }
        y_pos++;
        moveMotorsForward();
        x_pos = x_floating;
    } else {
        delayCondition = 1;
        if (distance < 10) {
            moveMotorsBackward();
        } else if (x_pos < 18) {
            if (x_floating < 34) {
                move_val = 1;
                moveMotorsRight();
                x_floating++;
            } else {
                x_pos = x_floating;
            }
        } else if (x_pos > 17) {
            if (x_floating > 0) {
                move_val = -1;
                moveMotorsLeft();
                x_floating--;
            } else {
                x_pos = x_floating;
            }
        }
    }

    if (y_pos > 140 || colorsensorValues[0] == GREEN) {
        stopMotors();
        Serial.println("Finished");
        delay(100000);
    }

    /////////////////////////////////////////////////////////////////////////////
    delay(100);
}
