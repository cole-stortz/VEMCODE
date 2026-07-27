// @board Arduino Uno
#include <Wire.h>

#define DEVICE_ADDR 0x68

void setup() {
    Serial.begin(9600);
    Wire.begin();
    Serial.println("Wire ready -- configure address 0x68 on the I2C tab");
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

    Serial.print("endTransmission status=");
    Serial.print(status);
    Serial.print("  bytes=");
    Serial.print(values[0]);
    Serial.print(",");
    Serial.println(values[1]);

    delay(500);
}
