// @board Arduino Uno
// Tests the String class -- UtilityTest only covers free functions
// (map/constrain/abs/min/max/random) and never exercises String at all.
void setup() {
    Serial.begin(9600);
    Serial.println("=== StringTest ===");

    String a = "Hello";
    String b = String(", ") + "VEMCODE" + String('!');
    String c = a + b;
    Serial.print("concat: ");
    Serial.println(c); // "Hello, VEMCODE!"

    c += " (added)";
    Serial.print("+=: ");
    Serial.println(c);

    Serial.print("length(): ");
    Serial.println(c.length());
    Serial.print("isEmpty() on c: ");
    Serial.println(c.isEmpty() ? "true" : "false"); // false
    Serial.print("isEmpty() on empty String: ");
    Serial.println(String("").isEmpty() ? "true" : "false"); // true

    Serial.print("== same text: ");
    Serial.println((String("abc") == String("abc")) ? "true" : "false"); // true
    Serial.print("!= different text: ");
    Serial.println((String("abc") != String("xyz")) ? "true" : "false"); // true
    Serial.print("equals(): ");
    Serial.println(a.equals("Hello") ? "true" : "false"); // true

    Serial.print("indexOf(\", \"): ");
    Serial.println(c.indexOf(", ")); // 5
    Serial.print("indexOf('!'): ");
    Serial.println(c.indexOf('!')); // 14

    Serial.print("startsWith(\"Hello\"): ");
    Serial.println(c.startsWith("Hello") ? "true" : "false"); // true
    Serial.print("endsWith(\"(added)\"): ");
    Serial.println(c.endsWith("(added)") ? "true" : "false"); // true
    Serial.print("contains(\"VEMCODE\"): ");
    Serial.println(c.contains("VEMCODE") ? "true" : "false"); // true

    Serial.print("substring(7, 14): ");
    Serial.println(c.substring(7, 14)); // "VEMCODE"
    Serial.print("replace(\"VEMCODE\",\"WORLD\"): ");
    Serial.println(c.replace("VEMCODE", "WORLD"));

    Serial.print("toLowerCase(): ");
    Serial.println(c.toLowerCase());
    Serial.print("toUpperCase(): ");
    Serial.println(c.toUpperCase());
    Serial.print("trim(\"  padded  \"): [");
    Serial.print(String("  padded  ").trim());
    Serial.println("]");

    Serial.print("charAt(0): ");
    Serial.println(a.charAt(0)); // 'H'
    Serial.print("operator[](1): ");
    Serial.println(a[1]); // 'e'
    a.setCharAt(0, 'J');
    Serial.print("after setCharAt(0,'J'): ");
    Serial.println(a); // "Jello"

    String num = "42";
    Serial.print("toInt(): ");
    Serial.println(num.toInt());
    String flt = "3.14";
    Serial.print("toFloat(): ");
    Serial.println(flt.toFloat());

    Serial.println("=== done ===");
}

void loop() {
    delay(1000);
}
