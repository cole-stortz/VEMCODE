// 128x64 I2C SSD1306 OLED -- watch the canvas bitmap update as text and a
// moving circle are drawn, cleared, and redrawn each frame.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("VEMCODE OLED");
  display.display();
  Serial.println("OLED ready");
}

void loop() {
  static int x = 0;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("x=");
  display.println(x);
  display.drawCircle(x, 32, 10, SSD1306_WHITE);
  display.drawRect(0, 50, SCREEN_WIDTH, 14, SSD1306_WHITE);
  display.display();
  Serial.print("OLED frame x=");
  Serial.println(x);
  delay(150);

  x += 5;
  if (x >= SCREEN_WIDTH) x = 0;
}
