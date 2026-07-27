// @board Arduino Uno
// Rotary encoder -- CLK/DT pair, direction resolved by comparing the two
// pins' states on each CLK change (an interrupt on CLK).
#define CLK_PIN 4
#define DT_PIN  5

volatile long encPos = 0;

void onEncoderChange() {
    if (digitalRead(CLK_PIN) != digitalRead(DT_PIN)) encPos++;
    else                                              encPos--;
}

void setup() {
    Serial.begin(9600);
    pinMode(CLK_PIN, INPUT_PULLUP);
    pinMode(DT_PIN,  INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(CLK_PIN), onEncoderChange, CHANGE);
}

void loop() {
    Serial.print("position: ");
    Serial.println(encPos);
    watch_variable("encPos", encPos);
    delay(100);
}
