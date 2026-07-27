// @board Arduino Uno
// IR obstacle-detection sensor -- a simple digital input.
#define IR_SENSOR_PIN 7

void setup() {
    Serial.begin(9600);
    pinMode(IR_SENSOR_PIN, INPUT);
}

void loop() {
    bool detected = digitalRead(IR_SENSOR_PIN);
    Serial.print("IR: ");
    Serial.println(detected ? "object detected" : "clear");
    delay(200);
}
