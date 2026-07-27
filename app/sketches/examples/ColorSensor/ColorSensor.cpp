// @board Arduino Uno
// TCS3200-style color sensor, array-declared pins (an alternative to plain
// #define pins for OUT/S2/S3 -- both forms are detected).
const int colorSensorOut[1] = {11};
const int colorSensorS2[1]  = {12};
const int colorSensorS3[1]  = {13};

void setup() {
    Serial.begin(9600);
    pinMode(colorSensorS2[0], OUTPUT);
    pinMode(colorSensorS3[0], OUTPUT);
    pinMode(colorSensorOut[0], INPUT);
}

void loop() {
    digitalWrite(colorSensorS2[0], LOW);
    digitalWrite(colorSensorS3[0], LOW);
    unsigned long colorPulse = pulseIn(colorSensorOut[0], LOW);

    Serial.print("color pulse: ");
    Serial.println(colorPulse);
    delay(200);
}
