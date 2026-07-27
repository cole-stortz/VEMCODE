#pragma once
#include <QString>
#include <QMap>
#include <QStringList>

// Quick-reference data backing the editor's right-click "API Reference" lookup.
// Content mirrors docs/API_REFERENCE.md -- kept short since this renders in a
// tooltip, not a full page; see that doc for the complete behavioral notes.
struct ApiFunctionDoc {
    QString signature;
    QString summary;
    QStringList params;  // "name -- description", empty if no params
    QString returns;      // empty if void
};

inline const QMap<QString, ApiFunctionDoc>& apiReferenceTable() {
    static const QMap<QString, ApiFunctionDoc> table = {
        {"pinMode", {"pinMode(pin, mode)",
            "Sets a pin's mode.",
            {"pin -- pin number", "mode -- INPUT, OUTPUT, or INPUT_PULLUP"}, ""}},
        {"digitalWrite", {"digitalWrite(pin, value)",
            "Sets a digital pin HIGH or LOW.",
            {"pin -- pin number", "value -- HIGH or LOW"}, ""}},
        {"digitalRead", {"digitalRead(pin)",
            "Reads a digital pin. Floating INPUT pins with nothing wired return random noise.",
            {"pin -- pin number"}, "int -- HIGH or LOW"}},
        {"analogWrite", {"analogWrite(pin, value)",
            "Writes a PWM value. Not clamped to a resolution or validated against which pins support PWM.",
            {"pin -- pin number", "value -- duty cycle, typically 0-255"}, ""}},
        {"analogRead", {"analogRead(pin)",
            "Reads an analog pin. Accepts either the A0-style constant or a bare channel index.",
            {"pin -- analog pin (A0, A1, ...) or channel index"}, "int -- 0-1023"}},
        {"analogReference", {"analogReference(mode)",
            "No-op; simulation ignores ADC reference voltage entirely.",
            {"mode -- DEFAULT, INTERNAL, or EXTERNAL"}, ""}},
        {"delay", {"delay(ms)",
            "Pauses the sketch. Scales with the speed slider; releases the interrupt lock every 10ms chunk so ISRs can still preempt it.",
            {"ms -- milliseconds"}, ""}},
        {"delayMicroseconds", {"delayMicroseconds(us)",
            "Busy-poll based, not a hardware timer, so it's not microsecond-precise.",
            {"us -- microseconds"}, ""}},
        {"millis", {"millis()",
            "Milliseconds since the sketch started. Scales with the speed slider, not wall-clock time.",
            {}, "unsigned long"}},
        {"micros", {"micros()",
            "Same as millis(), in microseconds.",
            {}, "unsigned long"}},
        {"map", {"map(value, fromLow, fromHigh, toLow, toHigh)",
            "Re-maps a number from one range to another. Returns toLow if fromLow == fromHigh instead of dividing by zero.",
            {"value", "fromLow, fromHigh -- source range", "toLow, toHigh -- target range"}, "long"}},
        {"constrain", {"constrain(value, min, max)",
            "Clamps a value to a range.",
            {"value", "min, max"}, "same type as value"}},
        {"abs", {"abs(value)", "Absolute value. Macro, so an argument with a side effect can evaluate twice.",
            {"value"}, "same type as value"}},
        {"min", {"min(a, b)", "Smaller of two values. Macro, same double-evaluation caveat as abs().",
            {"a", "b"}, "same type as a/b"}},
        {"max", {"max(a, b)", "Larger of two values. Macro, same double-evaluation caveat as abs().",
            {"a", "b"}, "same type as a/b"}},
        {"random", {"random(max) / random(min, max)",
            "Random long, thin wrapper over C's rand(). random(0) is undefined behavior, same as real Arduino.",
            {"max -- exclusive upper bound", "min -- optional inclusive lower bound"}, "long"}},
        {"randomSeed", {"randomSeed(seed)", "Seeds the random generator (srand()).",
            {"seed"}, ""}},
        {"tone", {"tone(pin, frequency, duration=0)",
            "Marks the buzzer active on the canvas; no actual audio is produced. duration=0 plays until noTone().",
            {"pin", "frequency -- Hz", "duration -- ms, optional"}, ""}},
        {"noTone", {"noTone(pin)", "Stops a tone and marks the buzzer inactive.", {"pin"}, ""}},
        {"pulseIn", {"pulseIn(pin, value, timeout=1000000UL)",
            "Measures a pulse. Returns an injected/canvas value immediately if one exists, otherwise polls for real (timeout in microseconds).",
            {"pin", "value -- HIGH or LOW to measure", "timeout -- microseconds, optional"}, "unsigned long -- pulse length in microseconds, 0 on timeout"}},
        {"Serial.begin", {"Serial.begin(baud)",
            "Initializes Serial. baud is stored but has no effect on timing simulation.",
            {"baud"}, ""}},
        {"Serial.print", {"Serial.print(value)", "Prints without a trailing newline.",
            {"value -- int, float, String, or const char*"}, ""}},
        {"Serial.println", {"Serial.println(value)", "Same as print(), with a trailing newline.",
            {"value"}, ""}},
        {"Serial.printf", {"Serial.printf(format, ...)", "printf-style formatting via vsnprintf.",
            {"format", "..."}, ""}},
        {"Serial.available", {"Serial.available()", "Bytes waiting to be read.", {}, "int -- 0 if none"}},
        {"Serial.read", {"Serial.read()", "Reads and removes the next byte.", {}, "int -- byte read, or -1 if empty"}},
        {"attachInterrupt", {"attachInterrupt(pin, callback, mode)",
            "Registers an interrupt handler. Accepts any pin, VEMCODE doesn't enforce the real Uno's pin 2/3-only restriction.",
            {"pin", "callback -- void function", "mode -- CHANGE, RISING, or FALLING"}, ""}},
        {"detachInterrupt", {"detachInterrupt(pin)", "Not implemented, calling it will fail to compile.", {"pin"}, ""}},
        {"noInterrupts", {"noInterrupts()", "Disables all interrupt/ISR dispatch (single global flag).", {}, ""}},
        {"interrupts", {"interrupts()", "Re-enables interrupt/ISR dispatch.", {}, ""}},
        {"Wire.begin", {"Wire.begin()", "No-op; no I2C slave mode.", {}, ""}},
        {"Wire.beginTransmission", {"Wire.beginTransmission(address)", "Starts queuing bytes for a transmission.",
            {"address"}, ""}},
        {"Wire.write", {"Wire.write(...)", "Queues a byte, string, or buffer to send.",
            {"value -- byte, const char*, or (buf, len)"}, ""}},
        {"Wire.endTransmission", {"Wire.endTransmission()", "Sends queued bytes. Always reports success, no bus errors are modeled.",
            {}, "int -- always 0"}},
        {"Wire.requestFrom", {"Wire.requestFrom(address, quantity)",
            "Requests bytes from the virtual I2C device panel for that address; missing bytes are zero-padded.",
            {"address", "quantity"}, "int -- bytes queued"}},
        {"Wire.available", {"Wire.available()", "Bytes waiting to be read.", {}, "int"}},
        {"Wire.read", {"Wire.read()", "Reads and removes the next byte.", {}, "int -- -1 if empty"}},
        {"SPI.begin", {"SPI.begin()", "No-op.", {}, ""}},
        {"SPI.beginTransaction", {"SPI.beginTransaction(settings)", "No-op; clock/bit order/data mode are ignored.",
            {"settings -- SPISettings"}, ""}},
        {"SPI.endTransaction", {"SPI.endTransaction()", "No-op.", {}, ""}},
        {"SPI.transfer", {"SPI.transfer(byte) / SPI.transfer(buf, len)",
            "Returns the next byte from the virtual SPI panel's configured sequence, wrapping around. The byte you send is ignored.",
            {"byte, or buf/len"}, "uint8_t, or void (buffer overwritten in place)"}},
        {"wdt_enable", {"wdt_enable(timeout_ms)",
            "Starts the watchdog. Takes a plain millisecond value, not real AVR's WDTO_* register enum (those constants still work, defined as plain ms values).",
            {"timeout_ms"}, ""}},
        {"wdt_disable", {"wdt_disable()", "Stops the watchdog.", {}, ""}},
        {"wdt_reset", {"wdt_reset()", "Resets the watchdog countdown.", {}, ""}},
        {"EEPROM.read", {"EEPROM.read(address)", "Reads a byte. Returns 0xFF if address is out of range (0-1023).",
            {"address"}, "uint8_t"}},
        {"EEPROM.write", {"EEPROM.write(address, value)", "Writes a byte. Silently does nothing if address is out of range.",
            {"address", "value"}, ""}},
        {"EEPROM.update", {"EEPROM.update(address, value)", "Same as write(), but skips the write if the value is already the same.",
            {"address", "value"}, ""}},

        // Library methods below are keyed by bare name rather than "Receiver.name" --
        // the receiver is a user-chosen variable (myServo, lcd, ...), not a fixed
        // keyword like Serial/Wire/SPI, so only names unique enough to identify
        // their class unambiguously are included here. Generic verbs shared across
        // several classes (write, read, begin, print, ...) are deliberately left
        // out rather than risk showing the wrong class's docs.
        {"attach", {"Servo::attach(pin)", "Sets the servo's pin. One-argument form only, no attach(pin, min, max).",
            {"pin"}, ""}},
        {"attached", {"Servo::attached()", "Whether the servo has a pin attached.", {}, "bool"}},
        {"detach", {"Servo::detach()", "Detaches the servo.", {}, ""}},
        {"getKey", {"Keypad::getKey()", "Scans the matrix and returns the currently pressed key.",
            {}, "char -- NO_KEY if nothing is pressed"}},
        {"readTemperature", {"DHT::readTemperature(fahrenheit=false)", "Reads the configured temperature.",
            {"fahrenheit -- optional, converts from Celsius"}, "float"}},
        {"readHumidity", {"DHT::readHumidity()", "Reads the configured relative humidity.", {}, "float -- percent"}},
        {"setLed", {"LedControl::setLed(addr, row, col, state)", "Sets a single LED in the matrix.",
            {"addr -- device index", "row, col -- 0-7", "state -- on/off"}, ""}},
        {"setRow", {"LedControl::setRow(addr, row, value)", "Sets a whole row via an 8-bit mask.",
            {"addr", "row", "value"}, ""}},
        {"setColumn", {"LedControl::setColumn(addr, col, value)", "Sets a whole column via an 8-bit mask.",
            {"addr", "col", "value"}, ""}},
        {"clearDisplay", {"LedControl::clearDisplay(addr)", "Clears one device in the chain.", {"addr"}, ""}},
        {"setIntensity", {"LedControl::setIntensity(addr, intensity)", "Accepted as a no-op, no brightness simulation.",
            {"addr", "intensity"}, ""}},
        {"shutdown", {"LedControl::shutdown(addr, status)", "Accepted as a no-op.", {"addr", "status"}, ""}},
        {"setCursor", {"LiquidCrystal::setCursor(col, row)", "Moves the write cursor. Clamped to 0-15/0-1 (always 16x2).",
            {"col", "row"}, ""}},
        {"createChar", {"LiquidCrystal::createChar(num, data)", "Accepted but does nothing, custom glyphs aren't rendered.",
            {"num", "data -- 8-byte glyph"}, ""}},
        {"listen", {"SoftwareSerial::listen()", "Always returns true, multi-instance RX contention isn't simulated.",
            {}, "bool -- always true"}},
        {"isListening", {"SoftwareSerial::isListening()", "Always returns true.", {}, "bool -- always true"}},
        {"overflow", {"SoftwareSerial::overflow()", "Always returns false.", {}, "bool -- always false"}},
    };
    return table;
}

inline const ApiFunctionDoc* lookupApiFunction(const QString& word) {
    const auto& table = apiReferenceTable();
    auto it = table.find(word);
    return it != table.end() ? &it.value() : nullptr;
}
