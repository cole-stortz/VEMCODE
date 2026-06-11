// @board Teensy 4.1
// Teensy 4.1 has Serial, Serial1, and Serial2.
// Serial1/Serial2 output goes to the debug console (std::cout) -- the canvas
// serial monitor only shows Serial output for now.

void setup() {
    Serial.begin(9600);
    Serial1.begin(9600);
    Serial2.begin(9600);

    Serial.println("=== MultiSerialTest ===");
    Serial.println("Serial1 and Serial2 output appears in the app console");

    Serial1.println("Hello from Serial1");
    Serial2.println("Hello from Serial2");

    Serial.println("setup done");
}

void loop() {
    Serial.println("main loop tick");
    Serial1.println("Serial1 tick");
    Serial2.println("Serial2 tick");
    delay(1000);
}