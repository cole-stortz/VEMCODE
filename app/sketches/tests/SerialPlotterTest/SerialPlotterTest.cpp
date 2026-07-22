// Exercises the Serial Plotter panel: two named values on one line (the
// Arduino IDE Serial Plotter "label:value" format) so they show up as two
// separate colored traces sharing one Y axis. Open the "Serial plotter" tab
// in the debug panel while this runs.
void setup() {
  Serial.begin(9600);
}

void loop() {
  float t = millis() / 1000.0;
  float wave1 = sin(t) * 50 + 50;   // 0..100
  float wave2 = cos(t) * 20;        // -20..20

  Serial.print("Wave1:");
  Serial.print(wave1);
  Serial.print(",Wave2:");
  Serial.println(wave2);

  delay(50);
}
