// @board Arduino Uno

#define TRIG_PIN 9
#define ECHO_PIN 10
#define LED_PIN  13

void setup() {
    Serial.begin(9600);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH);
    float cm = duration * 0.034f / 2.0f;

    Serial.print("distance: ");
    Serial.print(cm);
    Serial.println(" cm");

    // Light the LED when something gets within 10cm
    digitalWrite(LED_PIN, (cm > 0 && cm < 10) ? HIGH : LOW);

    delay(200);
}
