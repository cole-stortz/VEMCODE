// @board Arduino Uno
// Tests AVR-specific macros and headers that VEMCODE emulates on x86.
//
// #include <avr/pgmspace.h>  stripped; pgm_read_byte/word/dword/float defined as pointer deref
// #include <util/delay.h>    stripped; _delay_ms/_delay_us defined as api->delay/delayMicroseconds
// PROGMEM                    defined as empty -- the attribute is a no-op on x86
// F("string")                defined as (x) -- passes the string literal through unchanged
// F_CPU                      defined as 16000000UL
// analogReference(mode)      stubbed as a no-op
// #ifdef ARDUINO             ARDUINO defined as 100 (same value as Arduino IDE)
//
// Expected output: all checks pass with the correct values printed.

#include <avr/pgmspace.h>
#include <util/delay.h>

const char PROGMEM greeting[] = "hello";

void setup() {
    Serial.begin(9600);
    Serial.println("=== AVRCompatTest ===");

    // F() macro -- should pass the string through unchanged
    Serial.println(F("F() macro: ok"));

    // PROGMEM + pgm_read_byte -- reads bytes from a const array
    char buf[6];
    for (int i = 0; i < 5; i++)
        buf[i] = (char)pgm_read_byte(&greeting[i]);
    buf[5] = '\0';
    Serial.print("pgm_read_byte: ");
    Serial.println(buf);  // "hello"

    // pgm_read_word -- reads 16-bit value
    const uint16_t PROGMEM vals[] = {0xABCD, 0x1234};
    uint16_t w0 = pgm_read_word(&vals[0]);
    Serial.print("pgm_read_word[0]: 0x");
    Serial.println(w0, 16);  // ABCD

    // F_CPU
    Serial.print("F_CPU: ");
    Serial.println((long)F_CPU);  // 16000000

    // _delay_ms -- maps to api->delay()
    unsigned long t0 = millis();
    _delay_ms(50);
    Serial.print("_delay_ms(50) elapsed: ");
    Serial.println(millis() - t0);  // ~50

    // _delay_us -- maps to api->delayMicroseconds()
    _delay_us(1000);
    Serial.println("_delay_us(1000): ok");

    // analogReference -- no-op stub, should not crash
    analogReference(DEFAULT);
    Serial.println("analogReference(DEFAULT): ok");

    // #ifdef ARDUINO -- ARDUINO is defined as 100
    #ifdef ARDUINO
    Serial.print("ARDUINO defined: ");
    Serial.println(ARDUINO);  // 100
    #else
    Serial.println("ARDUINO not defined -- FAIL");
    #endif

    Serial.println("=== done ===");
}

void loop() {
    delay(1000);
}
