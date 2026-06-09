// @board Arduino Uno

void setup() {
    Serial.begin(9600);

    // --- String construction ---
    String s1 = "Hello";
    String s2 = " World";
    String s3 = s1 + s2;
    Serial.println(s3);                      // Hello World

    // --- String from numeric types ---
    String fromInt    = String(42);
    String fromFloat  = String(3.14f);
    Serial.println(fromInt);                 // 42
    Serial.println(fromFloat);              // 3.140000

    // --- Concatenation and += ---
    String built = "Count: ";
    built += 7;
    Serial.println(built);                   // Count: 7

    // --- Comparison ---
    Serial.println(s1 == "Hello" ? "eq pass" : "eq FAIL");
    Serial.println(s1 != s2     ? "neq pass" : "neq FAIL");
    Serial.println(s1.equals("Hello") ? "equals pass" : "equals FAIL");

    // --- Info ---
    Serial.println(s3.length());            // 11
    Serial.println(s3.isEmpty() ? "empty" : "not empty");

    // --- Search ---
    Serial.println(s3.indexOf("World"));    // 6
    Serial.println(s3.startsWith("Hello") ? "starts pass" : "starts FAIL");
    Serial.println(s3.endsWith("World")   ? "ends pass"   : "ends FAIL");
    Serial.println(s3.contains("lo W")    ? "contains pass" : "contains FAIL");

    // --- Manipulation ---
    Serial.println(s3.substring(6));        // World
    Serial.println(s3.substring(0, 5));     // Hello
    Serial.println(String("  trim me  ").trim());
    Serial.println(String("abc").toUpperCase());
    Serial.println(String("XYZ").toLowerCase());
    Serial.println(String("aXbXc").replace("X", "-"));

    // --- charAt / operator[] ---
    Serial.println(s3.charAt(0));           // H
    Serial.println(s3[6]);                  // W

    // --- Conversion ---
    String numStr = "123";
    Serial.println(numStr.toInt());         // 123
    String floatStr = "2.5";
    Serial.println(floatStr.toFloat());     // 2.5

    // --- Serial format specifiers ---
    int val = 255;
    Serial.print("HEX: "); Serial.println(val, HEX);   // ff
    Serial.print("OCT: "); Serial.println(val, OCT);   // 377
    Serial.print("BIN: "); Serial.println(val, BIN);   // 11111111
    Serial.print("DEC: "); Serial.println(val, DEC);   // 255

    // --- Serial float precision ---
    float f = 3.14159f;
    Serial.print("2dp: "); Serial.println(f, 2);        // 3.14
    Serial.print("4dp: "); Serial.println(f, 4);        // 3.1416
}

void loop() {
    delay(1000);
}
