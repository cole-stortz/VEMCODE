// @board Arduino Uno

#define BUTTON_PIN  2
#define SWITCH_PIN  3
#define POT_PIN     A0
#define LDR_PIN     A1
#define TEMP_PIN    A2
#define CLK_PIN     4
#define DT_PIN      5
#define JOY_VRX_PIN A3
#define JOY_VRY_PIN A4
#define JOY_SW_PIN  6

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
    attachInterrupt(CLK_PIN, onEncoderChange, CHANGE);
    Serial.println("InputTest ready — interact with components on the canvas");
}

void loop() {
    bool btn  = digitalRead(BUTTON_PIN) == LOW;
    bool sw   = digitalRead(SWITCH_PIN);
    int  pot  = analogRead(POT_PIN);
    int  ldr  = analogRead(LDR_PIN);
    int  temp = analogRead(TEMP_PIN);
    int  joyX = analogRead(JOY_VRX_PIN);
    int  joyY = analogRead(JOY_VRY_PIN);
    bool joySw = digitalRead(JOY_SW_PIN) == LOW;

    Serial.print("BTN:"); Serial.print(btn);
    Serial.print("  SW:"); Serial.print(sw);
    Serial.print("  POT:"); Serial.print(pot);
    Serial.print("  LDR:"); Serial.print(ldr);
    Serial.print("  TEMP:"); Serial.print(temp);
    Serial.print("  ENC:"); Serial.print(encPos);
    Serial.print("  JOY:"); Serial.print(joyX); Serial.print(",");
    Serial.print(joyY); Serial.print(",");
    Serial.println(joySw);

    watch_variable("button", btn);
    watch_variable("switch", sw);
    watch_variable("pot", pot);
    watch_variable("ldr", ldr);
    watch_variable("temp", temp);
    watch_variable("encoder", encPos);
    watch_variable("joyX", joyX);
    watch_variable("joyY", joyY);
    watch_variable("joySw", joySw);

    delay(200);
}
