// @board Arduino Uno
// SoftwareSerial's begin/print/println/available/read/peek/write. Output is
// routed to the serial monitor prefixed "[SW:RX_PIN]".
#include <SoftwareSerial.h>

#define RX_PIN 10
#define TX_PIN 11

SoftwareSerial mySerial(RX_PIN, TX_PIN);

void setup() {
    Serial.begin(9600);
    mySerial.begin(9600);
    mySerial.println("hello from SoftwareSerial");
}

void loop() {
    mySerial.print("tick ");
    mySerial.println(millis());
    delay(1000);
}
