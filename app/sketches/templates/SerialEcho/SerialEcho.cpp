// @board Arduino Uno

void setup() {
    Serial.begin(9600);
    Serial.println("Ready");
}

void loop() {
    if (Serial.available() > 0) {
        String input = "";
        while (Serial.available() > 0) {
            input += (char)Serial.read();
        }
        Serial.print("Echo: ");
        Serial.println(input);
    }
}
