// @board Teensy 4.1
// Tests multi-port serial output. Teensy 4.1 has serial_count=3 so the
// Serial monitor panel splits into three panes: Serial | Serial1 | Serial2.

void setup() {
    Serial.begin(9600);
    Serial1.begin(9600);
    Serial2.begin(9600);

    Serial.println("=== MultiSerialTest ===");
    Serial1.println("=== Serial1 ready ===");
    Serial2.println("=== Serial2 ready ===");

    Serial.println("setup done");
}

void loop() {
    Serial.println("main loop tick");
    Serial1.println("Serial1 tick");
    Serial2.println("Serial2 tick");
    delay(1000);
}