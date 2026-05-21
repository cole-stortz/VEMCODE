// VirtualBench multi-pin detection test

#define TRIG_PIN 11
#define ECHO_PIN 12

#define MOTOR1_PWM 3
#define MOTOR1_CWISE 4
#define MOTOR1_ANTI_CWISE 5

const int numSensors = 1;
const int S0[numSensors] = {6};
const int S1[numSensors] = {7};
const int S2[numSensors] = {8};
const int S3[numSensors] = {9};
const int sensorOut[numSensors] = {10};

long duration;
int distance;

void setup() {
    Serial.begin(9600);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    pinMode(MOTOR1_PWM, OUTPUT);
    pinMode(MOTOR1_CWISE, OUTPUT);
    pinMode(MOTOR1_ANTI_CWISE, OUTPUT);

    for (int i = 0; i < numSensors; i++) {
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
    // HC-SR04 read
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    duration = pulseIn(ECHO_PIN, HIGH, 1000000);
    distance = duration * 0.034 / 2;
    Serial.print("Distance: ");
    Serial.println(distance);

    // Motor forward
    analogWrite(MOTOR1_PWM, 100);
    digitalWrite(MOTOR1_CWISE, HIGH);
    digitalWrite(MOTOR1_ANTI_CWISE, LOW);

    delay(500);
}
