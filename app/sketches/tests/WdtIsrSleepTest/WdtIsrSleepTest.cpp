// @board Arduino Uno
// Exercises sleep_cpu() waking via a registered WDT_vect ISR handler (rather
// than the no-handler on_watchdog_reset callback path) to confirm the
// watchdog thread can still fire the ISR after waking the sketch thread.
volatile int wdtCount = 0;

ISR(WDT_vect) {
    wdtCount++;
    Serial.print("WDT ISR fired, count=");
    Serial.println(wdtCount);
}

void setup() {
    Serial.begin(9600);
    wdt_enable(WDTO_250MS);
    sleep_enable();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

void loop() {
    Serial.println("sleeping...");
    unsigned long before = millis();
    sleep_cpu();
    Serial.print("woke after ");
    Serial.print(millis() - before);
    Serial.println("ms");
}
