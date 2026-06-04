#define LED_PIN        13
#define BUTTON_PIN     2
#define SWITCH_PIN     3
#define BUZZER_PIN     4
#define POT_PIN        A0
#define PHOTO_PIN      A1
#define TEMP_PIN       A2

int counter     = 0;
int led_state   = 0;
int switch_state = 0;

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN,    OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(SWITCH_PIN, INPUT_PULLUP);
    Serial.println("VEMCODE component test ready");
}

void loop() {
    // Button -- hold to turn LED on
    int btn = digitalRead(BUTTON_PIN);
    if (btn == LOW) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Button held -- LED on");
    } else {
        digitalWrite(LED_PIN, LOW);
    }

    // Switch -- toggle buzzer on/off
    int sw = digitalRead(SWITCH_PIN);
    if (sw != switch_state) {
        switch_state = sw;
        digitalWrite(BUZZER_PIN, sw == LOW ? HIGH : LOW);
        Serial.println(sw == LOW ? "Switch ON" : "Switch OFF");
    }

    // Potentiometer -- read and watch
    int pot_val = analogRead(POT_PIN);
    watch_variable("pot", pot_val);

    // Light sensor -- read and watch
    int light_val = analogRead(PHOTO_PIN);
    watch_variable("light", light_val);

    // Temp sensor -- read and watch
    int temp_val = analogRead(TEMP_PIN);
    watch_variable("temp", temp_val);

    // Counter
    counter++;
    watch_variable("counter", counter);

    delay(100);
}