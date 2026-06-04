// VEMCODE sensor injection test

// Distance sensor
#define TRIG_PIN 11
#define ECHO_PIN 12

// Light sensor (analog)
#define PHOTO_PIN A0

// Temp sensor (analog)
#define TEMP_PIN A1

// Generic analog sensor
#define SENSOR_PIN A2

// Color sensor arrays
const int numSensors = 1;
const int S0[numSensors]        = {2};
const int S1[numSensors]        = {3};
const int S2[numSensors]        = {4};
const int S3[numSensors]        = {5};
const int sensorOut[numSensors] = {6};

void setup() {
    Serial.begin(9600);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(PHOTO_PIN, INPUT);
    pinMode(TEMP_PIN, INPUT);
    pinMode(SENSOR_PIN, INPUT);

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
    // Distance
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 1000000);
    int distance = duration * 0.034 / 2;
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println("cm");

    // Light
    int light = analogRead(PHOTO_PIN);
    Serial.print("Light: ");
    Serial.println(light);

    // Temp
    int temp = analogRead(TEMP_PIN);
    Serial.print("Temp raw: ");
    Serial.println(temp);

    // Generic analog
    int sensor = analogRead(SENSOR_PIN);
    Serial.print("Sensor: ");
    Serial.println(sensor);

    // Color
    digitalWrite(S2[0], LOW);
    digitalWrite(S3[0], LOW);
    long red = pulseIn(sensorOut[0], LOW, 1000000);

    digitalWrite(S2[0], HIGH);
    digitalWrite(S3[0], HIGH);
    long green = pulseIn(sensorOut[0], LOW, 1000000);

    digitalWrite(S2[0], LOW);
    digitalWrite(S3[0], HIGH);
    long blue = pulseIn(sensorOut[0], LOW, 1000000);

    Serial.print("Red: ");
    Serial.print(red);
    Serial.print(" Green: ");
    Serial.print(green);
    Serial.print(" Blue: ");
    Serial.println(blue);

}
