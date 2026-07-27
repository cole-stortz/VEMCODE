// @board Arduino Uno
// Seven-segment display -- one pin per segment (A-G), no decimal point.
#define SEG_A 2
#define SEG_B 3
#define SEG_C 4
#define SEG_D 5
#define SEG_E 6
#define SEG_F 7
#define SEG_G 8

void setup() {
    Serial.begin(9600);
    pinMode(SEG_A, OUTPUT);
    pinMode(SEG_B, OUTPUT);
    pinMode(SEG_C, OUTPUT);
    pinMode(SEG_D, OUTPUT);
    pinMode(SEG_E, OUTPUT);
    pinMode(SEG_F, OUTPUT);
    pinMode(SEG_G, OUTPUT);
}

void showDigit(bool a, bool b, bool c, bool d, bool e, bool f, bool g) {
    digitalWrite(SEG_A, a);
    digitalWrite(SEG_B, b);
    digitalWrite(SEG_C, c);
    digitalWrite(SEG_D, d);
    digitalWrite(SEG_E, e);
    digitalWrite(SEG_F, f);
    digitalWrite(SEG_G, g);
}

void loop() {
    showDigit(1, 1, 1, 0, 0, 0, 0); // "7"
    Serial.println("7");
    delay(500);
    showDigit(1, 1, 1, 1, 1, 1, 1); // "8"
    Serial.println("8");
    delay(500);
    showDigit(0, 0, 0, 0, 0, 0, 0); // blank
    Serial.println("blank");
    delay(500);
}
