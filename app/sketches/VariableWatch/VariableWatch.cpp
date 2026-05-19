#define LED_PIN 13

int counter = 0;
int led_state = 0;

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
    counter++;
    Serial.println(counter);
    watch_variable("counter", counter);
    watch_variable("led_state", led_state);
    delay(500);
}