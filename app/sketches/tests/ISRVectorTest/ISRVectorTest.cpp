// @board Arduino Uno
// Tests ISR() vector macro transforms.
//
// The preprocessor strips ISR(X_vect) { } wrappers and renames each body to
// __vb_isr_X_vect(), then injects api->register_isr() calls into vb_setup().
//
// How to test:
//   USART_RX_vect: type anything in the serial monitor and hit Send.
//                  Each character triggers the ISR -- watch rx_count climb.
//   INT0_vect:     click the Button on the canvas (pin 2).
//                  Each FALLING edge triggers the ISR -- watch int0_count climb.
//   Unknown vector: a warning is printed in the serial monitor (not a compile error).

#include <avr/interrupt.h>   // stripped silently by preprocessor
#include <avr/io.h>          // stripped silently by preprocessor

#define BUTTON_PIN 2

volatile int rx_count   = 0;
volatile int int0_count = 0;

ISR(USART_RX_vect) {
    rx_count++;
}

ISR(INT0_vect) {
    int0_count++;
}

// Uncomment to test the "unknown vector" warning (sketch still runs):
// ISR(TIMER2_OVF_vect) { }

static int last_rx   = -1;
static int last_int0 = -1;

void setup() {
    Serial.begin(9600);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("=== ISRVectorTest ===");
    Serial.println("Send serial input to fire USART_RX_vect.");
    Serial.println("Click the Button (pin 2) to fire INT0_vect.");
}

void loop() {
    if (rx_count != last_rx) {
        Serial.print("USART_RX_vect fired! rx_count = ");
        Serial.println(rx_count);
        last_rx = rx_count;
    }
    if (int0_count != last_int0) {
        Serial.print("INT0_vect fired!    int0_count = ");
        Serial.println(int0_count);
        last_int0 = int0_count;
    }
    watch_variable("rx_count",   rx_count);
    watch_variable("int0_count", int0_count);
    delay(50);
}
