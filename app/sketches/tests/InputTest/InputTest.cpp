// @board Teensy 4.1

#define BUTTON_PIN     2
#define SWITCH_PIN     3
#define CLK_PIN        4
#define DT_PIN         5
#define JOY_SW_PIN     6
#define IR_SENSOR_PIN  7
#define CLEAN_PIN      8
#define DIST_TRIG_PIN  9
#define DIST_ECHO_PIN  10
#define MYSTERY_PIN    25

#define POT_PIN     A0
#define LDR_PIN     A1
#define TEMP_PIN    A2
#define JOY_VRX_PIN A3
#define JOY_VRY_PIN A4
#define FORCE_PIN   A5
#define ADC_PIN     30

const int colorSensorOut[1] = {11};
const int colorSensorS2[1]  = {12};
const int colorSensorS3[1]  = {13};

volatile long encPos = 0;

void onEncoderChange() {
    if (digitalRead(CLK_PIN) != digitalRead(DT_PIN)) encPos++;
    else                                              encPos--;
}

void setup() {
    Serial.begin(9600);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(SWITCH_PIN, INPUT);
    pinMode(CLK_PIN, INPUT_PULLUP);
    pinMode(DT_PIN,  INPUT_PULLUP);
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    pinMode(IR_SENSOR_PIN, INPUT);
    pinMode(CLEAN_PIN, INPUT_PULLUP);
    pinMode(DIST_TRIG_PIN, OUTPUT);
    pinMode(DIST_ECHO_PIN, INPUT);
    pinMode(colorSensorS2[0], OUTPUT);
    pinMode(colorSensorS3[0], OUTPUT);
    pinMode(colorSensorOut[0], INPUT);
    pinMode(MYSTERY_PIN, INPUT);
    attachInterrupt(CLK_PIN, onEncoderChange, CHANGE);
    Serial.println("InputTest ready — interact with components on the canvas");
}

void loop() {
    bool btn    = digitalRead(BUTTON_PIN) == LOW;
    bool sw     = digitalRead(SWITCH_PIN);
    bool clean  = digitalRead(CLEAN_PIN) == LOW;
    bool ir     = digitalRead(IR_SENSOR_PIN);
    bool mystery = digitalRead(MYSTERY_PIN);
    int  pot    = analogRead(POT_PIN);
    int  ldr    = analogRead(LDR_PIN);
    int  temp   = analogRead(TEMP_PIN);
    int  force  = analogRead(FORCE_PIN);
    int  adc    = analogRead(ADC_PIN);
    int  joyX   = analogRead(JOY_VRX_PIN);
    int  joyY   = analogRead(JOY_VRY_PIN);
    bool joySw  = digitalRead(JOY_SW_PIN) == LOW;

    digitalWrite(DIST_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(DIST_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(DIST_TRIG_PIN, LOW);
    long duration = pulseIn(DIST_ECHO_PIN, HIGH);
    float distanceCm = duration * 0.034f / 2.0f;

    digitalWrite(colorSensorS2[0], LOW);
    digitalWrite(colorSensorS3[0], LOW);
    unsigned long colorPulse = pulseIn(colorSensorOut[0], LOW);

    Serial.print("BTN:"); Serial.print(btn);
    Serial.print("  SW:"); Serial.print(sw);
    Serial.print("  CLEAN:"); Serial.print(clean);
    Serial.print("  IR:"); Serial.print(ir);
    Serial.print("  MYSTERY:"); Serial.print(mystery);
    Serial.print("  POT:"); Serial.print(pot);
    Serial.print("  LDR:"); Serial.print(ldr);
    Serial.print("  TEMP:"); Serial.print(temp);
    Serial.print("  FORCE:"); Serial.print(force);
    Serial.print("  ADC:"); Serial.print(adc);
    Serial.print("  ENC:"); Serial.print(encPos);
    Serial.print("  JOY:"); Serial.print(joyX); Serial.print(",");
    Serial.print(joyY); Serial.print(",");
    Serial.print(joySw);
    Serial.print("  DIST:"); Serial.print(distanceCm);
    Serial.print("  COLOR:"); Serial.println(colorPulse);

    watch_variable("button", btn);
    watch_variable("switch", sw);
    watch_variable("cleanButton", clean);
    watch_variable("irSensor", ir);
    watch_variable("mystery", mystery);
    watch_variable("pot", pot);
    watch_variable("ldr", ldr);
    watch_variable("temp", temp);
    watch_variable("force", force);
    watch_variable("adc", adc);
    watch_variable("encoder", encPos);
    watch_variable("joyX", joyX);
    watch_variable("joyY", joyY);
    watch_variable("joySw", joySw);
    watch_variable("distanceCm", distanceCm);
    watch_variable("colorPulse", (long)colorPulse);

    delay(20);
}
