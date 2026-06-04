#define LED_PIN 13
#define POT_PIN A0

int counter = 0;  // add this

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    Serial.println("VEMCODE ready");
}

void loop() {
    int pot = analogRead(POT_PIN);
    int mapped = map(pot, 0, 1023, 0, 255);
    int clamped = constrain(mapped, 50, 200);
    watch_variable("pot", pot);
    watch_variable("mapped", mapped);
    watch_variable("clamped", clamped);
    Serial.println(pot);
    delay(100);
}