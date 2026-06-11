// @board Arduino Uno
#include <EEPROM.h>

void setup() {
    Serial.begin(9600);
    Serial.println("=== EEPROMTest ===");

    // write then read back
    EEPROM.write(0, 42);
    EEPROM.write(1, 255);
    EEPROM.write(2, 0);

    Serial.print("addr 0 = "); Serial.println(EEPROM.read(0));   // 42
    Serial.print("addr 1 = "); Serial.println(EEPROM.read(1));   // 255
    Serial.print("addr 2 = "); Serial.println(EEPROM.read(2));   // 0

    // update: only writes if value changed
    EEPROM.update(0, 42);   // same value -- no write
    EEPROM.update(0, 99);   // different -- writes
    Serial.print("addr 0 after update = "); Serial.println(EEPROM.read(0));  // 99

    // out-of-bounds read should return 0xFF (safe, no crash)
    uint8_t oob = EEPROM.read(2000);
    Serial.print("oob read (expect 255) = "); Serial.println(oob);

    Serial.println("=== done ===");
}

void loop() {
    delay(1000);
}