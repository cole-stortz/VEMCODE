// @board Arduino Uno
// Tests wdt_enable/wdt_reset/wdt_disable. The runtime only produces an
// observable watchdog-reset effect while the sketch is inside sleep_cpu()
// (see ArduinoRuntime::impl_wdt_enable) -- a timeout with no active sleep_cpu()
// call has no user-visible effect, matching the classic low-power pattern of
// enabling the watchdog right before sleeping. Headless mode prints
// "WATCHDOG RESET" (wired in main.cpp) when the runtime fires the callback.
bool done = false;

void setup() {
    Serial.begin(9600);
    Serial.println("=== WatchdogTest ===");
    wdt_enable(WDTO_500MS);
}

// loop() must actually return every call -- VEMCODE's Stop button, hot-reload,
// and headless Ctrl+C all depend on that (SketchThread's dispatch loop can
// only check "should I stop?" between calls to loop(), same as real Arduino).
// A `while (true) delay(...)` tail inside loop() blocks that forever and hangs
// the whole app -- confirmed live: after Ctrl+C the process kept spinning at
// climbing CPU% and never exited. Guard the one-shot logic with a flag instead.
void loop() {
    if (done) { delay(1000); return; }
    done = true;

    // Phase 1: feed the watchdog faster than its timeout -- should never fire
    Serial.println("feeding watchdog for 2s (should NOT reset)...");
    for (int i = 0; i < 8; i++) {
        wdt_reset();
        delay(250);
    }
    Serial.println("survived 2s of feeding -- watchdog did not fire, as expected");

    // Phase 2: stop feeding it and sleep -- the watchdog should fire and wake
    // the sleep ("WATCHDOG RESET" then "sleep: woke" printed). The last
    // wdt_reset() above happened right before this final delay(250), so only
    // ~250ms of the 500ms timeout is actually left by the time we start
    // timing here -- expect roughly that, not the full 500ms.
    Serial.println("going to sleep without resetting the watchdog again...");
    sleep_enable();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    unsigned long before = millis();
    sleep_cpu();
    Serial.print("woke up after ");
    Serial.print(millis() - before);
    Serial.println("ms (expect roughly 250ms, the remainder of the 500ms timeout)");
    sleep_disable();
    wdt_disable();

    Serial.println("=== done ===");
}
