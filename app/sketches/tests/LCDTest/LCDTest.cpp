// @board Arduino Uno
// Tests LiquidCrystal begin/print/setCursor/clear/write/createChar.
// Expected: row 0 shows "Hello, VEMCODE!" once; row 1 shows a counter that
// increments every second, confirmed via the on_lcd_print callback (headless
// mode prints "LCD[pin] rowN: ..." each time text changes).
#include <LiquidCrystal.h>

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
int counter = 0;

void setup() {
    Serial.begin(9600);
    Serial.println("=== LCDTest ===");

    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("Hello, VEMCODE!");
    lcd.write('!');            // direct write() call, not just via print()
    lcd.createChar(0, nullptr); // should not crash -- no-op stub

    Serial.println("wrote row 0 -- watch for LCD[7] row0 in output");
}

void loop() {
    lcd.setCursor(0, 1);
    lcd.print("count: ");
    lcd.print(counter);

    Serial.print("counter=");
    Serial.println(counter);

    counter++;
    if (counter % 5 == 0) {
        lcd.clear();
        Serial.println("cleared LCD");
    }

    delay(1000);
}
