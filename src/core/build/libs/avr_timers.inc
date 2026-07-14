// AVR hardware timer registers (ATmega328P: Uno/Nano) -- Timer1 (16-bit,
// drives pins 9/10) and Timer2 (8-bit, drives pins 11/3) as overloaded-
// operator globals, one struct type per register role (shared across both
// timers via `timerId`), routing through dedicated avr_timer_* accessors.
// Injected unconditionally alongside avr_registers.inc for the same reason:
// real sketches never write an explicit #include <avr/io.h> to key off of.
#define CS10 0
#define CS11 1
#define CS12 2
#define CS20 0
#define CS21 1
#define CS22 2

#define WGM10 0
#define WGM11 1
#define WGM12 3
#define WGM13 4
#define WGM20 0
#define WGM21 1
#define WGM22 3

#define COM1A0 6
#define COM1A1 7
#define COM1B0 4
#define COM1B1 5
#define COM2A0 6
#define COM2A1 7
#define COM2B0 4
#define COM2B1 5

#define TOIE1  0
#define OCIE1A 1
#define OCIE1B 2
#define TOIE2  0
#define OCIE2A 1
#define OCIE2B 2

struct AvrTccraReg {
    int timerId;
    operator uint8_t() const { return api->avr_timer_get_tccra(timerId); }
    AvrTccraReg& operator=(uint8_t v)  { api->avr_timer_set_tccra(timerId, v); return *this; }
    AvrTccraReg& operator|=(uint8_t v) { api->avr_timer_set_tccra(timerId, (uint8_t)*this | v); return *this; }
    AvrTccraReg& operator&=(uint8_t v) { api->avr_timer_set_tccra(timerId, (uint8_t)*this & v); return *this; }
    AvrTccraReg& operator^=(uint8_t v) { api->avr_timer_set_tccra(timerId, (uint8_t)*this ^ v); return *this; }
};

struct AvrTccrbReg {
    int timerId;
    operator uint8_t() const { return api->avr_timer_get_tccrb(timerId); }
    AvrTccrbReg& operator=(uint8_t v)  { api->avr_timer_set_tccrb(timerId, v); return *this; }
    AvrTccrbReg& operator|=(uint8_t v) { api->avr_timer_set_tccrb(timerId, (uint8_t)*this | v); return *this; }
    AvrTccrbReg& operator&=(uint8_t v) { api->avr_timer_set_tccrb(timerId, (uint8_t)*this & v); return *this; }
    AvrTccrbReg& operator^=(uint8_t v) { api->avr_timer_set_tccrb(timerId, (uint8_t)*this ^ v); return *this; }
};

struct AvrTimskReg {
    int timerId;
    operator uint8_t() const { return api->avr_timer_get_timsk(timerId); }
    AvrTimskReg& operator=(uint8_t v)  { api->avr_timer_set_timsk(timerId, v); return *this; }
    AvrTimskReg& operator|=(uint8_t v) { api->avr_timer_set_timsk(timerId, (uint8_t)*this | v); return *this; }
    AvrTimskReg& operator&=(uint8_t v) { api->avr_timer_set_timsk(timerId, (uint8_t)*this & v); return *this; }
    AvrTimskReg& operator^=(uint8_t v) { api->avr_timer_set_timsk(timerId, (uint8_t)*this ^ v); return *this; }
};

struct AvrTcntReg {
    int timerId;
    operator uint32_t() const { return api->avr_timer_get_tcnt(timerId); }
    AvrTcntReg& operator=(uint32_t v) { api->avr_timer_set_tcnt(timerId, v); return *this; }
};

struct AvrOcraReg {
    int timerId;
    operator uint32_t() const { return api->avr_timer_get_ocra(timerId); }
    AvrOcraReg& operator=(uint32_t v) { api->avr_timer_set_ocra(timerId, v); return *this; }
};

struct AvrOcrbReg {
    int timerId;
    operator uint32_t() const { return api->avr_timer_get_ocrb(timerId); }
    AvrOcrbReg& operator=(uint32_t v) { api->avr_timer_set_ocrb(timerId, v); return *this; }
};

static AvrTccraReg TCCR1A{1};
static AvrTccrbReg TCCR1B{1};
static AvrTcntReg  TCNT1 {1};
static AvrOcraReg  OCR1A {1};
static AvrOcrbReg  OCR1B {1};
static AvrTimskReg TIMSK1{1};

static AvrTccraReg TCCR2A{2};
static AvrTccrbReg TCCR2B{2};
static AvrTcntReg  TCNT2 {2};
static AvrOcraReg  OCR2A {2};
static AvrOcrbReg  OCR2B {2};
static AvrTimskReg TIMSK2{2};
