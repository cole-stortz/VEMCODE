#define LED_PIN      13
#define BUTTON_PIN   2
#define SWITCH_PIN   3
#define BUZZER_PIN   4
#define POT_PIN      A0

int counter     = 0;
int led_state   = 0;
int sw_state    = 0;
int last_sw     = 1;
String status   = "idle";

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN,    OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(SWITCH_PIN, INPUT_PULLUP);
    Serial.println("Ready");
}

void loop() {
    int btn = digitalRead(BUTTON_PIN);
    int sw  = digitalRead(SWITCH_PIN);
    int pot = analogRead(POT_PIN);

    // Button held -- LED on, count presses
    if (btn == LOW) {
        digitalWrite(LED_PIN, HIGH);
        status = "btn pressed";
    } else {
        digitalWrite(LED_PIN, LOW);
    }

    // Switch toggle -- buzzer follows switch state
    if (sw != last_sw) {
        last_sw = sw;
        sw_state = (sw == LOW) ? 1 : 0;
        digitalWrite(BUZZER_PIN, sw_state);
        status = sw_state ? "switch ON" : "switch OFF";
    }

    // Map pot to 0-100
    int pot_pct = map(pot, 0, 1023, 0, 100);
    pot_pct     = constrain(pot_pct, 0, 100);

    // Increment counter based on pot value
    counter += pot_pct / 50;

    // Watch all variables
    watch_variable("counter",  counter);
    watch_variable("pot_pct",  pot_pct);
    watch_variable("btn",      btn);
    watch_variable("sw_state", sw_state);

    // Print status every 10 loops
    if (counter % 10 == 0 && counter > 0) {
        String msg = String("count=") + String(counter) + " pot=" + String(pot_pct) + "%";
        Serial.println(msg);
    }

    delay(100);
}