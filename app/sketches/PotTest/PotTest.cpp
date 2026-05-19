#define LED_PIN    13
#define POT_PIN    A0

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    int val = analogRead(POT_PIN);
    watch_variable("pot", val);
    Serial.println(val);
    delay(50);
}