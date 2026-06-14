// @board Arduino Uno
// Tests inline AVR assembly transforms.
//
// VEMCODE's preprocessor handles these before compilation:
//   __asm__("nop")                           stripped silently
//   asm("nop")                               stripped silently
//   __asm__ __volatile__("nop" ::: "memory") stripped silently (constraint form)
//   asm volatile("nop")                      stripped silently
//   __asm__("cli") / ("sei")                 → api->noInterrupts() / api->interrupts()
//   __asm__("sleep") / ("wdr") / ("rjmp 0") stripped, note printed (Phase 8)
//   __asm__("unknown_instr")                 stripped, WARNING printed in serial monitor
//
// Expected serial output: 7 "ok" lines, then "=== done ===".
// Also check for a WARNING line about "clr r0" from the unknown-instruction path.

#define LED_PIN 13

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    Serial.println("=== AsmCompatTest ===");

    // 1. simplest nop form
    __asm__("nop");
    Serial.println("__asm__(nop): ok");

    // 2. asm keyword (without underscores)
    asm("nop");
    Serial.println("asm(nop): ok");

    // 3. __asm__ __volatile__ with memory clobber (most common real-world form)
    __asm__ __volatile__("nop" ::: "memory");
    Serial.println("__asm__ __volatile__ nop clobber: ok");

    // 4. asm volatile (without underscores)
    asm volatile("nop");
    Serial.println("asm volatile(nop): ok");

    // 5. cli → noInterrupts()
    __asm__ __volatile__("cli" ::: "memory");
    Serial.println("cli (noInterrupts): ok");

    // 6. sei → interrupts()
    __asm__ __volatile__("sei" ::: "memory");
    Serial.println("sei (interrupts): ok");

    // 7. Phase-8 instruction stripped with a note (not an error)
    __asm__("wdr");
    Serial.println("wdr (Phase 8 note): ok");

    // 8. Unknown instruction -- stripped with a WARNING in serial monitor
    __asm__("clr r0");
    Serial.println("clr r0 (unknown -- WARNING above): ok");

    Serial.println("=== done ===");
}

void loop() {
    delay(1000);
}
