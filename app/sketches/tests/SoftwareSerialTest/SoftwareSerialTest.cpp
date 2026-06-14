// @board Arduino Uno
// Tests the injected SoftwareSerial stub.
// Expected serial monitor output (prefixed [SW:2] and [SW:6]):
//   [SW:2] === SoftwareSerialTest ===
//   [SW:2] print int: 42
//   [SW:2] print float: 3.140000
//   [SW:2] print String: hello
//   [SW:6] second instance on rx=6
//   [SW:2] write byte: A
//   [SW:2] write buffer: hi
//   [SW:2] peek: 65  available: 1
//   [SW:2] read: 65  available after: 0
//   [SW:2] listen: 1  isListening: 1  overflow: 0
//   [SW:2] loop tick 0
//   [SW:2] loop tick 1
//   ...

#include <SoftwareSerial.h>

#define RX_PIN  2
#define TX_PIN  3
#define RX2_PIN 6
#define TX2_PIN 7

SoftwareSerial mySerial(RX_PIN, TX_PIN);
SoftwareSerial mySerial2(RX2_PIN, TX2_PIN);

int tickCount = 0;

void setup() {
    mySerial.begin(9600);
    mySerial2.begin(9600);

    mySerial.println("=== SoftwareSerialTest ===");

    // print overloads
    mySerial.print("print int: ");
    mySerial.println(42);

    mySerial.print("print float: ");
    mySerial.println(3.14f);

    mySerial.print("print String: ");
    mySerial.println(String("hello"));

    // second independent instance
    mySerial2.println("second instance on rx=6");

    // write: single byte ('A' = 65)
    mySerial.print("write byte: ");
    mySerial.write(65);
    mySerial.println();

    // write: two bytes via single-byte overload
    mySerial.print("write buffer: ");
    mySerial.write('h');
    mySerial.write('i');
    mySerial.println();

    // peek and read from injected RX buffer
    // (In a real sketch the buffer is filled by incoming serial data.
    //  Here we inject a byte via the runtime to exercise the path.)
    // We can't inject from sketch code directly, so we test the API shape:
    // available() should be 0, peek() and read() should return -1 when empty.
    mySerial.print("peek empty: ");
    mySerial.println(mySerial.peek());      // -1

    mySerial.print("read empty: ");
    mySerial.println(mySerial.read());      // -1

    mySerial.print("available empty: ");
    mySerial.println(mySerial.available()); // 0

    // listen / isListening / overflow stubs
    mySerial.print("listen: ");
    mySerial.println(mySerial.listen());
    mySerial.print("isListening: ");
    mySerial.println(mySerial.isListening());
    mySerial.print("overflow: ");
    mySerial.println(mySerial.overflow());
}

void loop() {
    mySerial.print("loop tick ");
    mySerial.println(tickCount);
    tickCount++;
    delay(1000);
}
