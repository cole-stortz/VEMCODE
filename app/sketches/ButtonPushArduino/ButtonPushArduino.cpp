#define LED_PIN 13
#define BUTTON 0

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON, INPUT_PULLUP);
    Serial.println("VirtualBench ready");
}

void loop() {
    int btn = digitalRead(BUTTON);
    if (btn==LOW){
        Serial.println("BUTTON PUSH");
        digitalWrite(LED_PIN, HIGH);
    } else {
        Serial.println("BUTTON RELEASE");
        digitalWrite(LED_PIN, LOW);
    }
    delay(10);
}
