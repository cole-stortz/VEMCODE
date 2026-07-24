// Single 8x8 MAX7219 LED matrix -- watch the canvas dot grid light up as
// setLed/setRow/setColumn are called.
#include <LedControl.h>

#define DIN_PIN 12
#define CLK_PIN 11
#define CS_PIN  10

LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 1);

void setup() {
  Serial.begin(9600);
  lc.shutdown(0, false);
  lc.setIntensity(0, 8);
  lc.clearDisplay(0);
  Serial.println("Max7219 ready");
}

void loop() {
  static int row = 0;
  static int col = 0;

  lc.setLed(0, row, col, true);
  Serial.print("Max7219 lit row ");
  Serial.print(row);
  Serial.print(" col ");
  Serial.println(col);
  delay(150);
  lc.setLed(0, row, col, false);

  col++;
  if (col > 7) {
    col = 0;
    row++;
    if (row > 7) row = 0;
  }
}
