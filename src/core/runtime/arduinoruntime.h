#pragma once
#include "src/core/runtime/arduinoapi.h"
#include "src/core/runtime/boardprofile.h"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <atomic>
#include <deque>
#include <map>
#include <array>
#include <random>
#include <new>
#include <vector>
#include <thread>

struct AvrTimerState {
    long long prescaler = 0; // 0 = stopped; decoded from TCCRxB's CS bits
    long long modulus;       // 65536 for Timer1 (16-bit), 256 for Timer2 (8-bit)
    long long ref_avr_us = 0;
    long long ref_ticks  = 0;
    long long last_check_raw = 0; // raw tick count as of the last background poll
    uint8_t  tccra = 0, tccrb = 0, timsk = 0;
    uint32_t ocra = 0, ocrb = 0;
    int pinA, pinB; // Arduino pins whose PWM duty OCRxA/OCRxB drive
    AvrTimerState(long long mod, int pa, int pb) : modulus(mod), pinA(pa), pinB(pb) {}
};

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
    std::mutex color_mtx_; // guards the three maps above -- written from the
                            // GUI thread (inject_color), read from the sketch
                            // thread (impl_pulseIn)
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
    int sleep_mode_ = 0;
    bool sleep_enabled_ = false;
    bool sleep_woken_ = false;
    std::condition_variable sleep_cv_;
    std::mutex sleep_mtx_;

    std::map<int, std::vector<uint8_t>> wire_devices_; // address -> configured response bytes
    std::mutex wire_mtx_; // guards wire_devices_ -- written from the GUI thread
                           // (inject_wire_device), read from the sketch thread
                           // (impl_wire_request_from)
    int wire_tx_address_ = -1;
    std::vector<uint8_t> wire_tx_buffer_;   // bytes written since beginTransmission
    std::deque<uint8_t> wire_rx_buffer_;    // bytes available to read since requestFrom

    std::vector<uint8_t> spi_response_bytes_; // configured response sequence, cycled by transfer()
    size_t spi_response_index_ = 0;
    std::mutex spi_mtx_; // guards the two fields above -- written from the GUI
                          // thread (inject_spi_bytes), read from the sketch thread (impl_spi_transfer)

    std::mutex timer_mtx_; // guards timer1_/timer2_ -- written from the sketch
                            // thread (register writes), read from the timer thread (poll_timer)
    AvrTimerState timer1_{65536, 9, 10}; // Timer1: 16-bit, OC1A=pin9, OC1B=pin10
    AvrTimerState timer2_{256, 11, 3};   // Timer2: 8-bit,  OC2A=pin11, OC2B=pin3

    std::mutex exec_mtx_; // serializes loop() against ISR handlers
};

class ArduinoRuntime {
public:
    ArduinoRuntime(BoardProfile profile = BOARD_UNO);
    ~ArduinoRuntime();
    ArduinoAPI get_api();
    RuntimeState& get_state() { return state_; }
    std::mutex& exec_mutex() { return state_.exec_mtx_; }

    // Callbacks set by SketchThread; if unset, impl_* functions fall back to std::cout.
    std::function<void(const std::string&)> on_serial_output;
    std::function<void(int pin, int value)> on_pin_changed;
    std::function<void(const std::string&, int)> on_variable_changed;
    std::function<void(int pin, int row, const std::string&)> on_lcd_print;
    std::function<void(const std::string&)> on_serial1_output;
    std::function<void(const std::string&)> on_serial2_output;
    std::function<void(int rxPin, const std::string&)> on_soft_serial_output;
    std::function<void()> on_watchdog_reset;
    std::function<void(bool)> on_sleep_changed;

    void inject_pin(int pin, int value);
    void inject_button_bounce(int pin, int finalValue);
    void set_analog_noise(bool enabled) { analog_noise_enabled_ = enabled; }

    // Must be called before the sketch DLL is unloaded -- wdt_thread_/timer_thread_
    // call function pointers into it (ISR handlers), so unloading first is a use-after-free.
    void stop_threads() { stop_wdt_thread(); stop_timer_thread(); stop_tone_threads(); }

    void inject_analog(int pin, int value) {
        // A0=14, A1=15... → index 0,1...
        int analog_index = (pin >= profile_.analog_offset) ? pin - profile_.analog_offset : pin;
        if (analog_index >= 0 && analog_index < profile_.analog_count)
            state_.analog_values[analog_index] = value;
    }
    void set_speed_multiplier(float speed);

    void request_stop() {
        stop_requested_ = true;
        state_.sleep_cv_.notify_all();
    }
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

    void reset_state() {
        // must fully join before destroying state_'s mutex/condvar
        stop_wdt_thread();
        stop_timer_thread();
        stop_tone_threads();
        state_.~RuntimeState();
        new (&state_) RuntimeState();
    }

    void inject_pulse_duration(int pin, unsigned long micros) {
        if (pin >= 0 && pin < profile_.pin_count)
            state_.pulse_durations_[pin] = micros;
    }

    void inject_color(int out_pin, int s2_pin, int s3_pin, int r, int g, int b) {
        auto to_period = [](int v) -> unsigned long {
            return (unsigned long)(387 - (v * 348 / 255));
        };
        std::lock_guard<std::mutex> lock(state_.color_mtx_);
        state_.color_channels_[out_pin][0] = to_period(r);
        state_.color_channels_[out_pin][1] = to_period(b);
        state_.color_channels_[out_pin][2] = 200;
        state_.color_channels_[out_pin][3] = to_period(g);
        state_.color_sensor_s2_[out_pin] = s2_pin;
        state_.color_sensor_s3_[out_pin] = s3_pin;
    }

    void inject_wire_device(int address, const std::vector<uint8_t>& bytes) {
        std::lock_guard<std::mutex> lock(state_.wire_mtx_);
        state_.wire_devices_[address] = bytes;
    }

    void inject_spi_bytes(const std::vector<uint8_t>& bytes) {
        std::lock_guard<std::mutex> lock(state_.spi_mtx_);
        state_.spi_response_bytes_ = bytes;
        state_.spi_response_index_ = 0;
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
    static void          impl_set_sleep_mode       (int mode);
    static void          impl_sleep_enable         ();
    static void          impl_sleep_disable          ();
    static void          impl_sleep_cpu            ();

    static void          impl_wire_begin_transmission(int address);
    static void          impl_wire_write             (uint8_t b);
    static int           impl_wire_end_transmission  ();
    static int           impl_wire_request_from      (int address, int quantity);
    static int           impl_wire_available          ();
    static int           impl_wire_read               ();

    static uint8_t       impl_spi_transfer             (uint8_t b);

    static AvrTimerState& timer_state(ArduinoRuntime* rt, int timerId);
    static long long      avr_scaled_micros(ArduinoRuntime* rt);

    static uint8_t       impl_avr_timer_get_tccra(int timerId);
    static void          impl_avr_timer_set_tccra(int timerId, uint8_t value);
    static uint8_t       impl_avr_timer_get_tccrb(int timerId);
    static void          impl_avr_timer_set_tccrb(int timerId, uint8_t value);
    static uint8_t       impl_avr_timer_get_timsk(int timerId);
    static void          impl_avr_timer_set_timsk(int timerId, uint8_t value);
    static uint32_t      impl_avr_timer_get_tcnt (int timerId);
    static void          impl_avr_timer_set_tcnt (int timerId, uint32_t value);
    static uint32_t      impl_avr_timer_get_ocra  (int timerId);
    static void          impl_avr_timer_set_ocra  (int timerId, uint32_t value);
    static uint32_t      impl_avr_timer_get_ocrb  (int timerId);
    static void          impl_avr_timer_set_ocrb  (int timerId, uint32_t value);

    void poll_timer(AvrTimerState& st, long long avr_us,
                     const char* ovf_vect, const char* compa_vect, const char* compb_vect,
                     int toie_bit, int ociea_bit, int ocieb_bit);
    void dispatch_isr(const char* vect_name);
    void ensure_timer_thread_running();

    // Must be joined before state_'s mutex/condvar are destroyed, or
    // reset_state()/~ArduinoRuntime() can destroy them mid-use.
    void stop_wdt_thread();
    std::thread wdt_thread_;
    std::atomic<bool> wdt_thread_stop_{false};

    void stop_timer_thread();
    std::thread timer_thread_;
    std::atomic<bool> timer_thread_stop_{false};
    std::atomic<bool> timer_thread_started_{false};

    void stop_tone_threads();
    std::mutex tone_threads_mtx_;
    std::vector<std::thread> tone_threads_;

    std::deque<char> serial_buffer_;
    BoardProfile profile_;
    std::atomic<bool> stop_requested_ = false;
    float speed_multiplier_ = 1.0f;
    std::mt19937 rng_{std::random_device{}()};
    bool analog_noise_enabled_ = false;
};