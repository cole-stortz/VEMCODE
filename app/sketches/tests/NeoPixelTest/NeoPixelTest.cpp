// Single WS2812B strip -- watch the canvas dot grid chase a lit pixel down
// the strip, one color at a time.
#include <Adafruit_NeoPixel.h>

#define LED_PIN   6
#define LED_COUNT 8

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(9600);
  strip.begin();
  strip.show();
  Serial.println("NeoPixel ready");
}

void loop() {
  static int i = 0;

  strip.clear();
  strip.setPixelColor(i, strip.Color(255, 0, 0));
  strip.show();
  Serial.print("NeoPixel lit pixel ");
  Serial.println(i);
  delay(150);

  i++;
  if (i >= LED_COUNT) i = 0;
}
