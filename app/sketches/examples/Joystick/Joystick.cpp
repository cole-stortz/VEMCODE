// @board Arduino Uno
// Analog joystick -- two analog axes (VRX/VRY) plus a digital press switch (SW).
#define JOY_VRX_PIN A3
#define JOY_VRY_PIN A4
#define JOY_SW_PIN  6

void setup() {
    Serial.begin(9600);
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
}

void loop() {
    int x = analogRead(JOY_VRX_PIN);
    int y = analogRead(JOY_VRY_PIN);
    bool pressed = digitalRead(JOY_SW_PIN) == LOW;

    Serial.print("x=");
    Serial.print(x);
    Serial.print(" y=");
    Serial.print(y);
    Serial.print(" sw=");
    Serial.println(pressed ? "pressed" : "released");
    delay(100);
}
