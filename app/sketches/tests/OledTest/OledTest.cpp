// 128x64 I2C SSD1306 OLEDs -- three displays, none with a dedicated reset
// pin (the common case for I2C breakout modules), so they should now
// daisy-chain on canvas: the first wires to the board's real SCL pin, and
// each of the other two wires to the previous display instead of the
// board, positioned next to it.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_SSD1306 display3(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  display1.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display2.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  display3.begin(SSD1306_SWITCHCAPVCC, 0x3E);

  display1.clearDisplay();
  display1.setTextSize(1);
  display1.setTextColor(SSD1306_WHITE);
  display1.setCursor(0, 0);
  display1.print("DISPLAY 1");
  display1.display();

  display2.clearDisplay();
  display2.setTextSize(1);
  display2.setTextColor(SSD1306_WHITE);
  display2.setCursor(0, 0);
  display2.print("DISPLAY 2");
  display2.display();

  display3.clearDisplay();
  display3.setTextSize(1);
  display3.setTextColor(SSD1306_WHITE);
  display3.setCursor(0, 0);
  display3.print("DISPLAY 3");
  display3.display();

  Serial.println("All 3 OLEDs ready");
}

void loop() {
  static int x = 0;

  display1.clearDisplay();
  display1.setCursor(0, 0);
  display1.print("D1 x=");
  display1.println(x);
  display1.drawCircle(x, 32, 10, SSD1306_WHITE);
  display1.display();

  int x2 = SCREEN_WIDTH - x;
  display2.clearDisplay();
  display2.setCursor(0, 0);
  display2.print("D2 x=");
  display2.println(x2);
  display2.drawRect(x2 - 8, 22, 16, 16, SSD1306_WHITE);
  display2.display();

  display3.clearDisplay();
  display3.setCursor(0, 0);
  display3.print("D3 frame ");
  display3.println(x / 5);
  display3.fillCircle(64, 32, (x % SCREEN_WIDTH) / 12, SSD1306_WHITE);
  display3.display();

  Serial.print("frame x=");
  Serial.println(x);
  delay(150);

  x += 5;
  if (x >= SCREEN_WIDTH) x = 0;
}
