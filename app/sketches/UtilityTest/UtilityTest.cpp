// @board Arduino Uno

void setup() {
    Serial.begin(9600);
    Serial.println("=== UtilityTest ===");

    // map
    Serial.print("map(512, 0, 1023, 0, 100) = ");
    Serial.println(map(512, 0, 1023, 0, 100));       // 50

    // constrain
    Serial.print("constrain(-5,  0, 100) = ");
    Serial.println(constrain(-5,  0, 100));           // 0
    Serial.print("constrain(50,  0, 100) = ");
    Serial.println(constrain(50,  0, 100));           // 50
    Serial.print("constrain(150, 0, 100) = ");
    Serial.println(constrain(150, 0, 100));           // 100

    // abs
    Serial.print("abs(-42) = ");
    Serial.println(abs(-42));                         // 42
    Serial.print("abs(42)  = ");
    Serial.println(abs(42));                          // 42

    // min / max
    Serial.print("min(10, 20) = ");
    Serial.println(min(10, 20));                      // 10
    Serial.print("max(10, 20) = ");
    Serial.println(max(10, 20));                      // 20

    Serial.println("=== live values in watch panel ===");
}

void loop() {
    int r1      = random(100);
    int r2      = random(50, 150);
    int mapped  = map(r1, 0, 99, 0, 255);
    int clamped = constrain(r2, 60, 90);
    int absval  = abs(r1 - 50);
    int mn      = min(r1, r2);
    int mx      = max(r1, r2);

    watch_variable("random(100)",      r1);
    watch_variable("random(50,150)",   r2);
    watch_variable("map",              mapped);
    watch_variable("constrain",        clamped);
    watch_variable("abs(r1-50)",       absval);
    watch_variable("min(r1,r2)",       mn);
    watch_variable("max(r1,r2)",       mx);

    Serial.print("r1="); Serial.print(r1);
    Serial.print(" r2="); Serial.print(r2);
    Serial.print(" map="); Serial.print(mapped);
    Serial.print(" constrain="); Serial.print(clamped);
    Serial.print(" abs="); Serial.print(absval);
    Serial.print(" min="); Serial.print(mn);
    Serial.print(" max="); Serial.println(mx);

    delay(500);
}
