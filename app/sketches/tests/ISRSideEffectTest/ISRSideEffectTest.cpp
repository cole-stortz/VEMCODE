// @board Arduino Uno
// Regression test for the cross-thread ISR dispatch bug (BUGFIXES_PENDING.md
// #1): a timer ISR calling digitalWrite/millis/Serial used to silently no-op
// since g_runtime was never set on the background timer thread.
volatile int compACount = 0;

ISR(TIMER1_COMPA_vect) {
    compACount++;
    digitalWrite(13, compACount % 2);
    Serial.print("ISR fired, count=");
    Serial.print(compACount);
    Serial.print(", millis=");
    Serial.println(millis());
}

void setup() {
    Serial.begin(9600);
    pinMode(13, OUTPUT);

    noInterrupts();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = 100;
    TCCR1B |= (1 << CS11); // prescaler 8
    TIMSK1 |= (1 << OCIE1A);
    interrupts();
}

void loop() {
    delay(200);
    Serial.print("loop: digitalRead(13)=");
    Serial.println(digitalRead(13));
}
