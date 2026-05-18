#pragma once
#include "src/core/runtime/arduinoapi.h"
#include <chrono>
#include <functional>
#include <string>

struct RuntimeState {
    int  pin_modes[20]    = {};
    int  pin_values[20]   = {};
    int  analog_values[8] = {};
    bool serial_started   = false;
    int  serial_baud      = 0;
    std::chrono::steady_clock::time_point start_time;
};

class ArduinoRuntime {
public:
    ArduinoRuntime();
    ArduinoAPI get_api();
    RuntimeState& get_state() { return state_; }

    // Callbacks -- set by SketchThread to receive output instead of std::cout.
    // If not set, impl_* functions fall back to std::cout (useful for testing).
    std::function<void(const std::string&)> on_serial_output;
    std::function<void(int pin, int value)> on_pin_changed;

private:
    RuntimeState state_;

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