// @board Arduino Uno
// Simulation realism test sketch
//
// Tests three features:
//   1. Floating pin  -- FLOAT_PIN set to INPUT, never injected from UI
//                       should print random 0/1 on every read
//   2. Button bounce -- BTN_PIN uses bounce simulation; hold the button and
//                       watch the serial output flicker during the 10ms window
//   3. Ideal button  -- TACT_PIN uses no bounce; transitions are always clean
//   4. Analog noise  -- POT_PIN reads a potentiometer; enable "Analog noise"
//                       in Settings to see ±2 ADC count jitter on the value

#define BTN_PIN   2    // bouncy button   (INPUT_PULLUP, LOW = pressed)
#define TACT_PIN  3    // ideal button    (INPUT_PULLUP, LOW = pressed)
#define FLOAT_PIN 4    // floating pin    (INPUT, nothing connected)
#define POT_PIN   A0   // analog sensor   (set value via canvas input)
#define LED_PIN   13

void setup() {
    pinMode(BTN_PIN,   INPUT_PULLUP);
    pinMode(TACT_PIN,  INPUT_PULLUP);
    pinMode(FLOAT_PIN, INPUT);
    pinMode(LED_PIN,   OUTPUT);
    Serial.begin(9600);
    Serial.println("SimRealism ready");
    Serial.println("BTN=bounce  TACT=clean  FLOAT=random  POT=analog");
}

static int prev_btn  = 1;
static int prev_tact = 1;
static int prev_flt  = -1;

void loop() {
    int btn  = digitalRead(BTN_PIN);
    int tact = digitalRead(TACT_PIN);
    int flt  = digitalRead(FLOAT_PIN);

    if (btn != prev_btn || tact != prev_tact || flt != prev_flt) {
        Serial.print("BTN:");
        Serial.print(btn);
        Serial.print("  TACT:");
        Serial.print(tact);
        Serial.print("  FLOAT:");
        Serial.println(flt);
        prev_btn  = btn;
        prev_tact = tact;
        prev_flt  = flt;
    }

    digitalWrite(LED_PIN, btn == 0 ? HIGH : LOW);
    delay(2);
}