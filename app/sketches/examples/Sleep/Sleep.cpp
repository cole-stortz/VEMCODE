// @board Arduino Uno
// set_sleep_mode/sleep_enable/sleep_cpu/sleep_disable. sleep_cpu() is a
// documented no-op if sleep hasn't been enabled -- confirmed here by timing
// it. The watchdog is used as the practical wake trigger in headless mode
// (the classic wdt_enable()+sleep_cpu() low-power pattern).
void setup() {
    Serial.begin(9600);
    Serial.println("=== Sleep ===");

    unsigned long t0 = millis();
    sleep_cpu();
    Serial.print("sleeping before enable: elapsed ");
    Serial.print(millis() - t0);
    Serial.println("ms (expect ~0, it's a no-op when disabled)");
}

bool done = false;

void loop() {
    if (done) { delay(1000); return; }
    done = true;

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    wdt_enable(WDTO_250MS);

    Serial.println("sleeping (watchdog will wake it in ~250ms)...");
    unsigned long before = millis();
    sleep_cpu();
    Serial.print("woke after ");
    Serial.print(millis() - before);
    Serial.println("ms");

    sleep_disable();

    unsigned long t1 = millis();
    sleep_cpu();
    Serial.print("sleeping after disable: elapsed ");
    Serial.print(millis() - t1);
    Serial.println("ms (expect ~0)");

    Serial.println("=== cycle done ===");
}
