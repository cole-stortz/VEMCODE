#pragma once
#include "src/core/runtime/arduinoapi.h"
#include "src/core/runtime/boardprofile.h"
#include <chrono>
#include <functional>
#include <string>
#include <atomic>
#include <deque>
#include <map>
#include <array>
#include <random>

struct RuntimeState {
    int  pin_modes[80]    = {};
    int  pin_values[80]   = {};
    int  analog_values[20] = {};
    bool serial_started   = false;
    int  serial_baud      = 0;
    int  pwm_values[80]    = {};
    unsigned long pulse_durations_[80] = {};
    std::map<int, std::array<unsigned long, 4>> color_channels_; // out_pin → [R,Blue,Clear,G]
    std::map<int, int> color_sensor_s2_; // out_pin → s2_pin
    std::map<int, int> color_sensor_s3_; // out_pin → s3_pin
    std::map<int, int> tone_frequencies_; // pin → frequency
    std::chrono::steady_clock::time_point start_time;
    std::map<int, void(*)()> interrupt_callbacks_; // pin → callback
    std::map<int, int> interrupt_modes_; // pin → mode
    bool interrupts_enabled_ = true;
    std::array<uint8_t, 1024> eeprom_;
    bool serial1_started   = false;
    int  serial1_baud_      = 0;
    std::deque<char> serial1_buffer_;
    bool serial2_started   = false;
    int  serial2_baud_      = 0;
    std::deque<char> serial2_buffer_;
    std::map<int, std::deque<char>> soft_serial_buffers_; // rxPin → RX buffer
    std::map<std::string, void(*)()> isr_handlers_;       // vector name → handler
    bool pin_driven[80] = {};  // true once a UI component has injected this pin
    int  pin_bounce_target[80] = {};
    std::map<int, std::chrono::steady_clock::time_point> pin_bounce_until_;
    bool wdt_enabled_ = false;
    int wdt_timeout_ms_ = 0;
    std::chrono::steady_clock::time_point wdt_last_reset_;

};

class ArduinoRuntime {
public:
    ArduinoRuntime(BoardProfile profile = BOARD_UNO);
    ArduinoAPI get_api();
    RuntimeState& get_state() { return state_; }

    // Callbacks set by SketchThread; if unset, impl_* functions fall back to std::cout.
    std::function<void(const std::string&)> on_serial_output;
    std::function<void(int pin, int value)> on_pin_changed;
    std::function<void(const std::string&, int)> on_variable_changed;
    std::function<void(int pin, int row, const std::string&)> on_lcd_print;
    std::function<void(const std::string&)> on_serial1_output;
    std::function<void(const std::string&)> on_serial2_output;
    std::function<void(int rxPin, const std::string&)> on_soft_serial_output;
    std::function<void()> on_watchdog_reset;

    void inject_pin(int pin, int value);
    void inject_button_bounce(int pin, int finalValue);
    void set_analog_noise(bool enabled) { analog_noise_enabled_ = enabled; }

    void inject_analog(int pin, int value) {
        // A0=14, A1=15... → index 0,1...
        int analog_index = (pin >= profile_.analog_offset) ? pin - profile_.analog_offset : pin;
        if (analog_index >= 0 && analog_index < profile_.analog_count)
            state_.analog_values[analog_index] = value;
    }
    void set_speed_multiplier(float speed);

    void request_stop() { stop_requested_ = true;  }
    void clear_stop()   { stop_requested_ = false; }

    void inject_serial(const std::string& data) {
        for (char c : data)
            serial_buffer_.push_back(c);
        if (!data.empty() && state_.interrupts_enabled_) {
            auto it = state_.isr_handlers_.find("USART_RX_vect");
            if (it != state_.isr_handlers_.end() && it->second) {
                state_.interrupts_enabled_ = false;
                it->second();
                state_.interrupts_enabled_ = true;
            }
        }
    }

    void inject_soft_serial(int rxPin, const std::string& data) {
        for (char c : data)
            state_.soft_serial_buffers_[rxPin].push_back(c);
    }

    void setProfile(BoardProfile p) { profile_ = p; }

    void inject_pulse_duration(int pin, unsigned long micros) {
        if (pin >= 0 && pin < profile_.pin_count)
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
    static void          impl_lcd_print      (int pin, int row, const char* text);
    static int           impl_Serial_available();
    static int           impl_Serial_read    ();
    static void          impl_tone           (int pin, int frequency, int duration_ms);
    static void          impl_noTone         (int pin);
    static void          impl_attachInterrupt (int pin, void (*callback)(), int mode);
    static void          impl_noInterrupts   ();
    static void          impl_interrupts     ();
    static void          impl_EEPROM_write   (int address, uint8_t value);
    static uint8_t       impl_EEPROM_read    (int address);
    static void          impl_EEPROM_update  (int address, uint8_t value);
    static void          impl_Serial1_begin  (int baud);
    static void          impl_Serial1_print  (const char* s);
    static void          impl_Serial1_println(const char* s);
    static void          impl_Serial2_begin  (int baud);
    static void          impl_Serial2_print  (const char* s);
    static void          impl_Serial2_println(const char* s);
    static void          impl_soft_serial_begin    (int rxPin, int baud);
    static void          impl_soft_serial_print    (int rxPin, const char* s);
    static void          impl_soft_serial_println  (int rxPin, const char* s);
    static int           impl_soft_serial_available(int rxPin);
    static int           impl_soft_serial_read     (int rxPin);
    static int           impl_soft_serial_peek     (int rxPin);
    static void          impl_register_isr         (const char* vector_name, void (*handler)());
    static void          impl_wdt_reset            ();
    static void          impl_wdt_enable           (int timeout_ms);
    static void          impl_wdt_disable          ();

    std::deque<char> serial_buffer_;
    BoardProfile profile_;
    std::atomic<bool> stop_requested_ = false;
    float speed_multiplier_ = 1.0f;
    std::mt19937 rng_{std::random_device{}()};
    bool analog_noise_enabled_ = false;
};