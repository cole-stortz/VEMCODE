// @board Arduino Uno

#define LED_PIN 13

unsigned long lastToggle = 0;
int ledState = LOW;

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    Serial.println("NonBlockingBlink started");
}

void loop() {
    if (millis() - lastToggle >= 500) {
        lastToggle = millis();
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        Serial.print("LED: ");
        Serial.println(ledState ? "ON" : "OFF");
    }
}
