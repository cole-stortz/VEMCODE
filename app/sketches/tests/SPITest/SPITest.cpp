// @board Arduino Uno
#include <SPI.h>

void setup() {
    Serial.begin(9600);
    SPI.begin();
    Serial.println("SPITest ready — configure response bytes on the SPI tab");
}

void loop() {
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    uint8_t a = SPI.transfer(0x00);
    uint8_t b = SPI.transfer(0x00);
    SPI.endTransaction();

    uint8_t buf[3] = {0, 0, 0};
    SPI.transfer(buf, 3);

    Serial.print("a=");
    Serial.print(a);
    Serial.print(" b=");
    Serial.print(b);
    Serial.print(" buf=");
    Serial.print(buf[0]);
    Serial.print(",");
    Serial.print(buf[1]);
    Serial.print(",");
    Serial.println(buf[2]);

    delay(500);
}
