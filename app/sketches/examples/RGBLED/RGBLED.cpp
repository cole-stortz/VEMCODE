// @board Arduino Uno
// RGB LED -- three PWM pins named with a shared RED/GREEN/BLUE suffix,
// cycling through colors.
#define RED_PIN   3
#define GREEN_PIN 8
#define BLUE_PIN 10

void setup() {
    Serial.begin(9600);
    pinMode(RED_PIN,   OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN,  OUTPUT);
}

void loop() {
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
