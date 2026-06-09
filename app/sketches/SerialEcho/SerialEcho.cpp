// @board Arduino Uno

void setup() {
    Serial.begin(9600);
    Serial.println("SerialEcho ready — type something in the serial monitor and press Send");
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
    delay(10);
}
