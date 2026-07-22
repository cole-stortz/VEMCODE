#pragma once
#include <cstdint>

// Cross-platform symbol export for sketch shared libraries
#ifdef _WIN32
#  define VB_EXPORT __declspec(dllexport)
#else
#  define VB_EXPORT __attribute__((visibility("default")))
#endif

// The Arduino API contract.
// This struct is filled by the host and injected into every sketch DLL.
// Adding new fields at the END is backwards-compatible.
struct ArduinoAPI {
    // GPIO
    void (*pinMode)   (int pin, int mode);
    void (*digitalWrite)(int pin, int value);
    int  (*digitalRead) (int pin);

    // Analog
    void (*analogWrite)(int pin, int value);
    int  (*analogRead) (int pin);

    // Timing
    void          (*delay)   (unsigned long ms);
    void (*delayMicroseconds)(unsigned long us);
    unsigned long (*millis)  ();
    unsigned long (*micros)  ();

    // Serial
    void (*Serial_begin)  (int baud);
    void (*Serial_print)  (const char* s);
    void (*Serial_println)(const char* s);

    // Variable Watch
    void (*watch_variable)(const char* name, int value);

    // Serial Communication
    int (*Serial_available)();
    int (*Serial_read)();

    // PulseIn
    unsigned long (*pulseIn)(int pin, int value, unsigned long timeout);

    // LCD display
    void (*lcd_print)(int pin, int row, const char* text);

    // DHT11/DHT22 -- canvas-injected readings, keyed by data pin
    float (*dht_read_temperature)(int pin);
    float (*dht_read_humidity)   (int pin);

    // Tone generation
    void (*tone)(int pin, int frequency, int duration_ms);
    void (*noTone)(int pin);

    // Interrupts
    void (*attachInterrupt)(int pin, void (*callback)(), int mode);
    void (*noInterrupts)();
    void (*interrupts)();

    // EEPROM read, write, and update
    void (*EEPROM_write)(int address, uint8_t value);
    uint8_t (*EEPROM_read)(int address);
    void (*EEPROM_update)(int address, uint8_t value);

    // Serial1 and Serial 2 for boards with multiple serial ports
    void (*Serial1_begin)(int baud);
    void (*Serial1_print)(const char* s);
    void (*Serial1_println)(const char* s);
    void (*Serial2_begin)(int baud);
    void (*Serial2_print)(const char* s);
    void (*Serial2_println)(const char* s);

    // SoftwareSerial -- keyed by RX pin so multiple instances are distinguishable
    void (*soft_serial_begin)    (int rxPin, int baud);
    void (*soft_serial_print)    (int rxPin, const char* s);
    void (*soft_serial_println)  (int rxPin, const char* s);
    int  (*soft_serial_available)(int rxPin);
    int  (*soft_serial_read)     (int rxPin);
    int  (*soft_serial_peek)     (int rxPin);

    // ISR vector registration -- called from vb_setup() for each ISR(X_vect) in the sketch
    void (*register_isr)(const char* vector_name, void (*handler)());

    // Watchdog
    void (*wdt_enable)(int timeout_ms);
    void (*wdt_disable)();
    void (*wdt_reset)();

    // Sleep
    void (*set_sleep_mode)(int mode);
    void (*sleep_enable)();
    void (*sleep_disable)();
    void (*sleep_cpu)();

    // Wire (I2C) -- master-mode, byte-level; responses come from a virtual
    // address->bytes table configured in the UI.
    void (*wire_begin_transmission)(int address);
    void (*wire_write)(uint8_t b);
    int  (*wire_end_transmission)();
    int  (*wire_request_from)(int address, int quantity);
    int  (*wire_available)();
    int  (*wire_read)();

    // SPI -- no per-device addressing; transfer() cycles through a configurable response sequence.
    uint8_t (*spi_transfer)(uint8_t b);

    // AVR hardware timers (Timer1/Timer2) -- one dedicated accessor pair per
    // register, timerId is 1 or 2
    void     (*avr_timer_set_tccra)(int timerId, uint8_t value);
    uint8_t  (*avr_timer_get_tccra)(int timerId);
    void     (*avr_timer_set_tccrb)(int timerId, uint8_t value);
    uint8_t  (*avr_timer_get_tccrb)(int timerId);
    void     (*avr_timer_set_tcnt) (int timerId, uint32_t value);
    uint32_t (*avr_timer_get_tcnt) (int timerId);
    void     (*avr_timer_set_ocra) (int timerId, uint32_t value);
    uint32_t (*avr_timer_get_ocra) (int timerId);
    void     (*avr_timer_set_ocrb) (int timerId, uint32_t value);
    uint32_t (*avr_timer_get_ocrb) (int timerId);
    void     (*avr_timer_set_timsk)(int timerId, uint8_t value);
    uint8_t  (*avr_timer_get_timsk)(int timerId);
};

// Namespaced to avoid clashing with windows.h's INPUT/HIGH/LOW macros.
// sketch.cpp pulls these in via "using namespace vb;"; host code doesn't.
namespace vb {
    constexpr int INPUT        = 0;
    constexpr int OUTPUT       = 1;
    constexpr int INPUT_PULLUP = 2;
    constexpr int LOW          = 0;
    constexpr int HIGH         = 1;

    // Interrupt modes
    constexpr int CHANGE  = 1;
    constexpr int FALLING = 2;
    constexpr int RISING  = 3;
}

// Analog pin constants -- global scope so they work in #define statements
// These map to the internal pin numbers used by the runtime (14-19)
constexpr int A0 = 14;
constexpr int A1 = 15;
constexpr int A2 = 16;
constexpr int A3 = 17;
constexpr int A4 = 18;
constexpr int A5 = 19;

// Sketch DLL must export vb_init/vb_setup/vb_loop (mapping to Arduino's
// init/setup()/loop()).