// @board Arduino Uno
#include <Wire.h>

#define DEVICE_ADDR 0x68

void setup() {
    Serial.begin(9600);
    Wire.begin();
    Serial.println("WireTest ready — configure address 0x68 on the Devices tab");
}

void loop() {
    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(0x00); // register pointer, ignored by the simulation
    uint8_t status = Wire.endTransmission();

    Wire.requestFrom(DEVICE_ADDR, 2);
    int values[2] = {0, 0};
    int i = 0;
    while (Wire.available() && i < 2) {
        values[i++] = Wire.read();
    }

    // exercise the write(buffer, len) overload and the optional stop-bit args
    uint8_t buf[3] = {1, 2, 3};
    Wire.beginTransmission(0x50);
    Wire.write(buf, 3);
    Wire.endTransmission(false);

    Serial.print("endTransmission status=");
    Serial.print(status);
    Serial.print("  bytes=");
    Serial.print(values[0]);
    Serial.print(",");
    Serial.println(values[1]);

    delay(500);
}
