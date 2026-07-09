// @board Arduino Uno
void setup() {
    Serial.begin(9600);
    DDRB |= (1 << PB5);   // pin 13 as OUTPUT via the port register, same as pinMode(13, OUTPUT)
    DDRD &= ~(1 << PD2);  // pin 2 as INPUT
    PORTD |= (1 << PD2);  // enable pull-up on pin 2
}

void loop() {
    PORTB |= (1 << PB5);  // pin 13 HIGH
    Serial.print("pin13 via digitalRead=");
    Serial.print(digitalRead(13));
    Serial.print(" PINB bit5=");
    Serial.println((PINB >> PB5) & 1);
    delay(300);

    PINB = (1 << PB5);    // toggle pin 13 via the AVR PINx-write quirk
    Serial.print("pin13 via digitalRead=");
    Serial.print(digitalRead(13));
    Serial.print(" PINB bit5=");
    Serial.println((PINB >> PB5) & 1);
    delay(300);

    Serial.print("pin2 via digitalRead=");
    Serial.print(digitalRead(2));
    Serial.print(" PIND bit2=");
    Serial.println((PIND >> PD2) & 1);
    delay(300);
}
