#define LED_PIN 13

int counter = 0;  // add this

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    Serial.println("VirtualBench ready");
}

void loop() {
    String msg = "Count: ";
    msg += counter;
    Serial.println(msg);
    counter++;
    delay(500);
}