// @board Arduino Uno
// wdt_enable/wdt_reset/wdt_disable. The runtime only produces an observable
// watchdog-reset effect while the sketch is inside sleep_cpu() -- a timeout
// with no active sleep_cpu() call has no user-visible effect, matching the
// classic low-power pattern of enabling the watchdog right before sleeping.
bool done = false;

void setup() {
    Serial.begin(9600);
    Serial.println("=== Watchdog ===");
    wdt_enable(WDTO_500MS);
}

// loop() must actually return every call -- the Stop button, hot-reload, and
// headless Ctrl+C all depend on that. Guard one-shot logic with a flag
// instead of blocking inside loop() with a while(true) tail.
void loop() {
    if (done) { delay(1000); return; }
    done = true;

    Serial.println("feeding watchdog for 2s (should NOT reset)...");
    for (int i = 0; i < 8; i++) {
        wdt_reset();
        delay(250);
    }
    Serial.println("survived 2s of feeding -- watchdog did not fire, as expected");

    Serial.println("going to sleep without resetting the watchdog again...");
    sleep_enable();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    unsigned long before = millis();
    sleep_cpu();
    Serial.print("woke up after ");
    Serial.print(millis() - before);
    Serial.println("ms");
    sleep_disable();
    wdt_disable();

    Serial.println("=== done ===");
}
