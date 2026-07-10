// @board Arduino Uno
// Tests set_sleep_mode/sleep_enable/sleep_cpu/sleep_disable in isolation.
// sleep_cpu() is a documented no-op if sleep hasn't been enabled -- confirmed
// here by timing it. The watchdog is only used as the practical wake trigger
// in headless mode (matching the classic wdt_enable()+sleep_cpu() low-power
// pattern) -- see WatchdogTest for watchdog-specific behavior.
void setup() {
    Serial.begin(9600);
    Serial.println("=== SleepTest ===");

    // sleep_cpu() without sleep_enable() first should return immediately.
    // (Note: avoid writing "sleep_cpu(" etc as literal text in Serial.print --
    // the preprocessor's token replacement isn't string-literal-aware and will
    // rewrite it to "api->sleep_cpu(" inside the printed message too.)
    unsigned long t0 = millis();
    sleep_cpu();
    Serial.print("sleeping before enable: elapsed ");
    Serial.print(millis() - t0);
    Serial.println("ms (expect ~0, proving it's a no-op when disabled)");
}

bool done = false;

// loop() must actually return every call -- VEMCODE's Stop button, hot-reload,
// and headless Ctrl+C all depend on that (the runtime can only check "should I
// stop?" between calls to loop(), same as real Arduino). A `while (true)
// delay(...)` tail inside loop() blocks that forever and hangs the whole app --
// confirmed live on WatchdogTest's identical pattern: after Ctrl+C the process
// kept spinning at climbing CPU% and never exited. Guard with a flag instead.
void loop() {
    if (done) { delay(1000); return; }
    done = true;

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    wdt_enable(WDTO_250MS); // only practical headless wake trigger available

    Serial.println("sleeping (watchdog will wake it in ~250ms)...");
    unsigned long before = millis();
    sleep_cpu();
    Serial.print("woke after ");
    Serial.print(millis() - before);
    Serial.println("ms");

    sleep_disable();

    // sleep_cpu() after sleep_disable() should also return immediately
    unsigned long t1 = millis();
    sleep_cpu();
    Serial.print("sleeping after disable: elapsed ");
    Serial.print(millis() - t1);
    Serial.println("ms (expect ~0)");

    Serial.println("=== cycle done ===");
}
