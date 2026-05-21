#pragma once
#include "src/core/runtime/arduinoapi.h"
#include <chrono>
#include <functional>
#include <string>
#include <atomic>
#include <deque>
#include <map>
#include <array>

struct RuntimeState {
    int  pin_modes[20]    = {};
    int  pin_values[20]   = {};
    int  analog_values[8] = {};
    bool serial_started   = false;
    int  serial_baud      = 0;
    int  pwm_values[20]    = {};
    unsigned long pulse_durations_[20] = {};
    std::map<int, std::array<unsigned long, 4>> color_channels_; // out_pin → [R,Blue,Clear,G]
    std::map<int, int> color_sensor_s2_; // out_pin → s2_pin
    std::map<int, int> color_sensor_s3_; // out_pin → s3_pin
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
    std::function<void(const std::string&, int)> on_variable_changed;

    void inject_pin(int pin, int value);

    void inject_analog(int pin, int value) {
        // Convert internal pin number to analog index
        // A0=14, A1=15... → index 0,1...
        int analog_index = (pin >= 14) ? pin - 14 : pin;
        if (analog_index >= 0 && analog_index < 8)
            state_.analog_values[analog_index] = value;
    }
    float speed_multiplier_ = 1.0f; // 0.5 = 2xspeed , 2.0 = 0.5xspeed
    void set_speed_multiplier(float speed);

    void inject_serial(const std::string& data) {
        for (char c : data)
            serial_buffer_.push_back(c);
    }

    void inject_pulse_duration(int pin, unsigned long micros) {
        if (pin >= 0 && pin < 20)
            state_.pulse_durations_[pin] = micros;
    }

    void inject_color(int out_pin, int s2_pin, int s3_pin, int r, int g, int b) {
        auto to_period = [](int v) -> unsigned long {
            return (unsigned long)(387 - (v * 348 / 255));
        };
        state_.color_channels_[out_pin][0] = to_period(r);
        state_.color_channels_[out_pin][1] = to_period(b);
        state_.color_channels_[out_pin][2] = 200;
        state_.color_channels_[out_pin][3] = to_period(g);
        state_.color_sensor_s2_[out_pin] = s2_pin;
        state_.color_sensor_s3_[out_pin] = s3_pin;
    }


    std::atomic<bool> stop_requested_ = false;

private:
    RuntimeState state_;

    static void          impl_pinMode        (int pin, int mode);
    static void          impl_digitalWrite   (int pin, int value);
    static int           impl_digitalRead    (int pin);
    static void          impl_analogWrite    (int pin, int value);
    static int           impl_analogRead     (int pin);
    static void          impl_delay          (unsigned long ms);
    static void       impl_delayMicroseconds (unsigned long us);
    static unsigned long impl_millis         ();
    static unsigned long impl_micros         ();
    static void          impl_Serial_begin   (int baud);
    static void          impl_Serial_print   (const char* s);
    static void          impl_Serial_println (const char* s);
    static void          impl_watch_variable (const char* name, int value);
    static unsigned long impl_pulseIn        (int pin, int value, unsigned long timeout);
    static int           impl_Serial_available();
    static int           impl_Serial_read    ();

    std::deque<char> serial_buffer_;
};