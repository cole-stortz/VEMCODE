#pragma once
#include "src/core/runtime/arduinoapi.h"
#include <chrono>

// Tracks all simulated hardware state for one simulation session.
// One instance lives inside ArduinoRuntime.
struct RuntimeState {
    int  pin_modes[20]   = {};  // INPUT / OUTPUT per pin
    int  pin_values[20]  = {};  // HIGH / LOW per pin
    int  analog_values[8]= {};  // Simulated analog pin values (A0-A7)
    bool serial_started  = false;
    int  serial_baud     = 0;
    std::chrono::steady_clock::time_point start_time;
};

// ArduinoRuntime implements the entire Arduino API as fake C++ functions.
// It owns RuntimeState and exposes a filled ArduinoAPI table for injection
// into sketch DLLs via SketchHost.
//
// All impl_* functions are static so their addresses can be stored in
// function pointer slots (non-static member functions can't be used that way).
// They reach state_ through a global pointer set in get_api().
class ArduinoRuntime {
public:
    ArduinoRuntime();

    // Returns an ArduinoAPI struct with every function pointer filled in.
    // Pass this to SketchHost::load() for injection into the sketch DLL.
    ArduinoAPI get_api();

    // Direct access to hardware state -- the UI will read this to update
    // the canvas, serial monitor, and signal timeline.
    RuntimeState& get_state() { return state_; }

private:
    RuntimeState state_;

    // Static implementations -- called by the sketch via function pointers.
    // They write to the global g_runtime pointer set in get_api().
    static void          impl_pinMode        (int pin, int mode);
    static void          impl_digitalWrite   (int pin, int value);
    static int           impl_digitalRead    (int pin);
    static void          impl_analogWrite    (int pin, int value);
    static int           impl_analogRead     (int pin);
    static void          impl_delay          (unsigned long ms);
    static unsigned long impl_millis         ();
    static unsigned long impl_micros         ();
    static void          impl_Serial_begin   (int baud);
    static void          impl_Serial_print   (const char* s);
    static void          impl_Serial_println (const char* s);
};