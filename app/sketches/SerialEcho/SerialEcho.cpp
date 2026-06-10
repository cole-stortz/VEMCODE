// @board Arduino Uno

#include <LiquidCrystal.h>

#define LCD_RS 12
#define LCD_EN 11
#define LCD_D4 5
#define LCD_D5 4
#define LCD_D6 3
#define LCD_D7 2

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

void setup() {
    Serial.begin(9600);
    lcd.begin(16, 2);
    lcd.print("SerialEcho ready");
    Serial.println("SerialEcho ready — type something in the serial monitor and press Send");
}

void loop() {
    if (Serial.available() > 0) {
        String input = "";
        while (Serial.available() > 0) {
            input += (char)Serial.read();
        }
        Serial.print("Echo: ");
        Serial.println(input);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(input.substring(0, 16));
        if (input.length() > 16) {
            lcd.setCursor(0, 1);
            lcd.print(input.substring(16, 32));
        }
    }
    delay(10);
}
