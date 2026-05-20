#pragma once
#include <cstdint>

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
};

// Arduino constants live in a namespace to avoid clashing with windows.h
// which defines INPUT as a struct and HIGH/LOW as macros.
// sketch.cpp pulls these in with "using namespace vb;".
// host.cpp never uses this namespace — it uses its own raw integers.
namespace vb {
    constexpr int INPUT        = 0;
    constexpr int OUTPUT       = 1;
    constexpr int INPUT_PULLUP = 2;
    constexpr int LOW          = 0;
    constexpr int HIGH         = 1;
}

// Analog pin constants -- global scope so they work in #define statements
// These map to the internal pin numbers used by the runtime (14-19)
constexpr int A0 = 14;
constexpr int A1 = 15;
constexpr int A2 = 16;
constexpr int A3 = 17;
constexpr int A4 = 18;
constexpr int A5 = 19;

// Sketch DLL must export these three symbols:
//   void vb_init(ArduinoAPI* api)   -- called once after LoadLibrary
//   void vb_setup()                 -- called once (maps to Arduino setup())
//   void vb_loop()                  -- called in a loop (maps to Arduino loop())