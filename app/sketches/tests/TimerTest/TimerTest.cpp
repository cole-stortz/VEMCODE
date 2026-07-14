// @board Arduino Uno
volatile unsigned long ovfCount = 0;
volatile unsigned long compACount = 0;
volatile unsigned long ovf2Count = 0;

ISR(TIMER1_OVF_vect) {
    ovfCount++;
}
ISR(TIMER1_COMPA_vect) {
    compACount++;
}
ISR(TIMER2_OVF_vect) {
    ovf2Count++;
}

void setup() {
    Serial.begin(9600);
    pinMode(9, OUTPUT);
    pinMode(11, OUTPUT);

    noInterrupts();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = 128;              // ~50% duty on pin 9 via the timer PWM path
    TCCR1B |= (1 << CS11);    // prescaler = 8
    TIMSK1 |= (1 << TOIE1) | (1 << OCIE1A);

    TCCR2A = 0;
    TCCR2B = 0;
    TCNT2 = 0;
    OCR2A = 64;               // ~25% duty on pin 11
    TCCR2B |= (1 << CS21);    // prescaler = 8
    TIMSK2 |= (1 << TOIE2);
    interrupts();
}

void loop() {
    Serial.print("TCNT1=");
    Serial.print(TCNT1);
    Serial.print(" ovf1=");
    Serial.print(ovfCount);
    Serial.print(" compA=");
    Serial.print(compACount);
    Serial.print(" TCNT2=");
    Serial.print(TCNT2);
    Serial.print(" ovf2=");
    Serial.println(ovf2Count);
    delay(500);
}
