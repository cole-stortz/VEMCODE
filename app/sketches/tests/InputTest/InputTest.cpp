// @board Arduino Uno

#define BUTTON_PIN  2
#define SWITCH_PIN  3
#define POT_PIN     A0
#define LDR_PIN     A1
#define TEMP_PIN    A2
#define CLK_PIN     4
#define DT_PIN      5

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
    attachInterrupt(CLK_PIN, onEncoderChange, CHANGE);
    Serial.println("InputTest ready — interact with components on the canvas");
}

void loop() {
    bool btn  = digitalRead(BUTTON_PIN) == LOW;
    bool sw   = digitalRead(SWITCH_PIN);
    int  pot  = analogRead(POT_PIN);
    int  ldr  = analogRead(LDR_PIN);
    int  temp = analogRead(TEMP_PIN);

    Serial.print("BTN:"); Serial.print(btn);
    Serial.print("  SW:"); Serial.print(sw);
    Serial.print("  POT:"); Serial.print(pot);
    Serial.print("  LDR:"); Serial.print(ldr);
    Serial.print("  TEMP:"); Serial.print(temp);
    Serial.print("  ENC:"); Serial.println(encPos);

    watch_variable("button", btn);
    watch_variable("switch", sw);
    watch_variable("pot", pot);
    watch_variable("ldr", ldr);
    watch_variable("temp", temp);
    watch_variable("encoder", encPos);

    delay(200);
}
