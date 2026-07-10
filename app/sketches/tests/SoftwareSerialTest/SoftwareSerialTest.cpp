// @board Arduino Uno
// Tests SoftwareSerial begin/print/println/available/read/peek/write.
// Output is routed to the serial monitor prefixed "[SW:RX_PIN]" per the
// runtime's stdout fallback (impl_soft_serial_print/println) -- no injection
// panel exists yet for feeding data back in, so available()/read()/peek()
// are expected to report empty (0 / -1) since nothing was ever received.
#include <SoftwareSerial.h>

#define RX_PIN 10
#define TX_PIN 11

SoftwareSerial mySerial(RX_PIN, TX_PIN);

void setup() {
    Serial.begin(9600);
    Serial.println("=== SoftwareSerialTest ===");

    mySerial.begin(9600);
    mySerial.println("hello from SoftwareSerial");
    mySerial.print("no newline, then: ");
    mySerial.write((uint8_t)'X');
    mySerial.println();

    Serial.print("available() with nothing injected: ");
    Serial.println(mySerial.available()); // 0
    Serial.print("read() with nothing injected: ");
    Serial.println(mySerial.read());      // -1
    Serial.print("peek() with nothing injected: ");
    Serial.println(mySerial.peek());      // -1
    Serial.print("listen()/isListening()/overflow(): ");
    Serial.print(mySerial.listen() ? "true " : "false ");
    Serial.print(mySerial.isListening() ? "true " : "false ");
    Serial.println(mySerial.overflow() ? "true" : "false");
}

void loop() {
    mySerial.print("tick ");
    mySerial.println(millis());
    delay(1000);
}
