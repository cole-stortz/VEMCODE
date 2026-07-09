// AVR direct port registers (ATmega328P layout: Uno/Nano) -- DDRx/PORTx/PINx
// as overloaded-operator globals routing through the same api->pinMode/
// digitalWrite/digitalRead calls as the normal Arduino API, so port-register
// writes and digitalWrite/pinMode calls observe each other's effects on the
// same simulated pin. Unlike Wire/SPI this is injected unconditionally (not
// gated on an #include) since real sketches using DDRB/PORTB/etc almost never
// write an explicit #include <avr/io.h> -- they get it transitively via
// Arduino.h, so there's no include line to key off of.
#define PB0 0
#define PB1 1
#define PB2 2
#define PB3 3
#define PB4 4
#define PB5 5
#define PB6 6
#define PB7 7
#define PC0 0
#define PC1 1
#define PC2 2
#define PC3 3
#define PC4 4
#define PC5 5
#define PC6 6
#define PC7 7
#define PD0 0
#define PD1 1
#define PD2 2
#define PD3 3
#define PD4 4
#define PD5 5
#define PD6 6
#define PD7 7

struct AvrPortState {
    uint8_t ddr = 0;
    uint8_t port = 0;
    const int8_t* pins; // Arduino pin number per bit, -1 if not exposed
};

inline void avr_apply_ddr(AvrPortState& st, uint8_t newDdr) {
    for (int bit = 0; bit < 8; ++bit) {
        int pin = st.pins[bit];
        if (pin < 0) continue;
        bool wasOut = st.ddr & (1 << bit);
        bool isOut  = newDdr & (1 << bit);
        if (wasOut == isOut) continue;
        if (isOut) api->pinMode(pin, 1 /*OUTPUT*/);
        else       api->pinMode(pin, (st.port & (1 << bit)) ? 2 /*INPUT_PULLUP*/ : 0 /*INPUT*/);
    }
    st.ddr = newDdr;
}

inline void avr_apply_port(AvrPortState& st, uint8_t newPort) {
    for (int bit = 0; bit < 8; ++bit) {
        int pin = st.pins[bit];
        if (pin < 0) continue;
        bool wasSet = st.port & (1 << bit);
        bool isSet  = newPort & (1 << bit);
        if (wasSet == isSet) continue;
        if (st.ddr & (1 << bit)) api->digitalWrite(pin, isSet ? 1 : 0);
        else                     api->pinMode(pin, isSet ? 2 /*INPUT_PULLUP*/ : 0 /*INPUT*/);
    }
    st.port = newPort;
}

struct AvrDdrReg {
    AvrPortState* st;
    operator uint8_t() const { return st->ddr; }
    AvrDdrReg& operator=(uint8_t v)  { avr_apply_ddr(*st, v); return *this; }
    AvrDdrReg& operator|=(uint8_t v) { avr_apply_ddr(*st, st->ddr | v); return *this; }
    AvrDdrReg& operator&=(uint8_t v) { avr_apply_ddr(*st, st->ddr & v); return *this; }
    AvrDdrReg& operator^=(uint8_t v) { avr_apply_ddr(*st, st->ddr ^ v); return *this; }
};

struct AvrPortReg {
    AvrPortState* st;
    operator uint8_t() const { return st->port; }
    AvrPortReg& operator=(uint8_t v)  { avr_apply_port(*st, v); return *this; }
    AvrPortReg& operator|=(uint8_t v) { avr_apply_port(*st, st->port | v); return *this; }
    AvrPortReg& operator&=(uint8_t v) { avr_apply_port(*st, st->port & v); return *this; }
    AvrPortReg& operator^=(uint8_t v) { avr_apply_port(*st, st->port ^ v); return *this; }
};

struct AvrPinReg {
    AvrPortState* st;
    operator uint8_t() const {
        uint8_t v = 0;
        for (int bit = 0; bit < 8; ++bit) {
            int pin = st->pins[bit];
            if (pin >= 0 && api->digitalRead(pin)) v |= (1 << bit);
        }
        return v;
    }
    // Real AVR quirk: writing a 1 bit to PINx toggles the corresponding PORTx bit.
    AvrPinReg& operator=(uint8_t v) { avr_apply_port(*st, st->port ^ v); return *this; }
};

static const int8_t AVR_PINS_B[8] = {8, 9, 10, 11, 12, 13, -1, -1};
static const int8_t AVR_PINS_C[8] = {14, 15, 16, 17, 18, 19, -1, -1};
static const int8_t AVR_PINS_D[8] = {0, 1, 2, 3, 4, 5, 6, 7};

static AvrPortState avr_state_b{0, 0, AVR_PINS_B};
static AvrPortState avr_state_c{0, 0, AVR_PINS_C};
static AvrPortState avr_state_d{0, 0, AVR_PINS_D};

static AvrDdrReg  DDRB{&avr_state_b};
static AvrPortReg PORTB{&avr_state_b};
static AvrPinReg  PINB{&avr_state_b};
static AvrDdrReg  DDRC{&avr_state_c};
static AvrPortReg PORTC{&avr_state_c};
static AvrPinReg  PINC{&avr_state_c};
static AvrDdrReg  DDRD{&avr_state_d};
static AvrPortReg PORTD{&avr_state_d};
static AvrPinReg  PIND{&avr_state_d};
