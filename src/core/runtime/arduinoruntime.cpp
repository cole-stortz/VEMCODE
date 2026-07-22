#include "src/core/runtime/arduinoruntime.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
//#include <algorithm>
#include <QtGlobal>

thread_local ArduinoRuntime* g_runtime = nullptr;

namespace {
    long long avr_floor_div(long long a, long long b) {
        long long q = a / b, r = a % b;
        return (r != 0 && ((r < 0) != (b < 0))) ? q - 1 : q;
    }

    // Number of times the free-running tick counter crosses a value congruent
    // to `target` mod `modulus` while advancing from prevRaw to currRaw --
    // i.e. how many overflow/compare-match events happened since the last poll.
    long long avr_count_crossings(long long prevRaw, long long currRaw, long long target, long long modulus) {
        if (currRaw <= prevRaw) return 0;
        return avr_floor_div(currRaw - target, modulus) - avr_floor_div(prevRaw - target, modulus);
    }

    // CS12:10 / CS22:20 prescaler decode (bits 2:0 of TCCRxB); 0 = stopped.
    long long avr_timer1_prescaler(uint8_t tccrb) {
        static const long long table[8] = {0, 1, 8, 64, 256, 1024, 0, 0};
        return table[tccrb & 0x07];
    }
    long long avr_timer2_prescaler(uint8_t tccrb) {
        static const long long table[8] = {0, 1, 8, 32, 64, 128, 256, 1024};
        return table[tccrb & 0x07];
    }

    long long avr_raw_ticks(const AvrTimerState& st, long long avr_us) {
        if (st.prescaler <= 0) return st.ref_ticks;
        long long delta_cycles = (avr_us - st.ref_avr_us) * 16; // F_CPU = 16MHz
        return st.ref_ticks + delta_cycles / st.prescaler;
    }
}

static std::string ts() {
    if (!g_runtime) return "[?ms] ";
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() -
        g_runtime->get_state().start_time).count();
    std::ostringstream ss;
    ss << "[" << std::setw(8) << us / 1000 << "ms] ";
    return ss.str();
}

ArduinoRuntime::ArduinoRuntime(BoardProfile profile) : profile_(profile) {
    state_.start_time = std::chrono::steady_clock::now();
}

ArduinoRuntime::~ArduinoRuntime() {
    stop_wdt_thread();
    stop_timer_thread();
    stop_tone_threads();
}

// Must run before state_'s mutex/condvar are destroyed -- an unjoined
// std::thread calls std::terminate() on destruction.
void ArduinoRuntime::stop_wdt_thread() {
    wdt_thread_stop_ = true;
    if (wdt_thread_.joinable())
        wdt_thread_.join();
}

ArduinoAPI ArduinoRuntime::get_api() {
    g_runtime = this;
    state_.start_time = std::chrono::steady_clock::now();
    ArduinoAPI api;
    api.pinMode        = impl_pinMode;
    api.digitalWrite   = impl_digitalWrite;
    api.digitalRead    = impl_digitalRead;
    api.analogWrite    = impl_analogWrite;
    api.analogRead     = impl_analogRead;
    api.delay          = impl_delay;
    api.millis         = impl_millis;
    api.micros         = impl_micros;
    api.Serial_begin   = impl_Serial_begin;
    api.Serial_print   = impl_Serial_print;
    api.Serial_println = impl_Serial_println;
    api.watch_variable = impl_watch_variable;
    api.Serial_available = impl_Serial_available;
    api.Serial_read      = impl_Serial_read;
    api.pulseIn          = impl_pulseIn;
    api.delayMicroseconds = impl_delayMicroseconds;
    api.lcd_print        = impl_lcd_print;
    api.tone             = impl_tone;
    api.noTone           = impl_noTone;
    api.attachInterrupt = impl_attachInterrupt;
    api.interrupts     = impl_interrupts;
    api.noInterrupts   = impl_noInterrupts;
    api.EEPROM_read    = impl_EEPROM_read;
    api.EEPROM_write   = impl_EEPROM_write;
    api.EEPROM_update  = impl_EEPROM_update;
    api.Serial1_begin  = impl_Serial1_begin;
    api.Serial1_print  = impl_Serial1_print;
    api.Serial1_println= impl_Serial1_println;
    api.Serial2_begin  = impl_Serial2_begin;
    api.Serial2_print  = impl_Serial2_print;
    api.Serial2_println= impl_Serial2_println;
    api.soft_serial_begin     = impl_soft_serial_begin;
    api.soft_serial_print     = impl_soft_serial_print;
    api.soft_serial_println   = impl_soft_serial_println;
    api.soft_serial_available = impl_soft_serial_available;
    api.soft_serial_read      = impl_soft_serial_read;
    api.soft_serial_peek      = impl_soft_serial_peek;
    api.register_isr          = impl_register_isr;
    api.wdt_reset             = impl_wdt_reset;
    api.wdt_enable            = impl_wdt_enable;
    api.wdt_disable           = impl_wdt_disable;
    api.set_sleep_mode        = impl_set_sleep_mode;
    api.sleep_enable          = impl_sleep_enable;
    api.sleep_disable         = impl_sleep_disable;
    api.sleep_cpu             = impl_sleep_cpu;
    api.wire_begin_transmission = impl_wire_begin_transmission;
    api.wire_write              = impl_wire_write;
    api.wire_end_transmission   = impl_wire_end_transmission;
    api.wire_request_from       = impl_wire_request_from;
    api.wire_available          = impl_wire_available;
    api.wire_read               = impl_wire_read;
    api.spi_transfer            = impl_spi_transfer;
    api.avr_timer_get_tccra     = impl_avr_timer_get_tccra;
    api.avr_timer_set_tccra     = impl_avr_timer_set_tccra;
    api.avr_timer_get_tccrb     = impl_avr_timer_get_tccrb;
    api.avr_timer_set_tccrb     = impl_avr_timer_set_tccrb;
    api.avr_timer_get_timsk     = impl_avr_timer_get_timsk;
    api.avr_timer_set_timsk     = impl_avr_timer_set_timsk;
    api.avr_timer_get_tcnt      = impl_avr_timer_get_tcnt;
    api.avr_timer_set_tcnt      = impl_avr_timer_set_tcnt;
    api.avr_timer_get_ocra      = impl_avr_timer_get_ocra;
    api.avr_timer_set_ocra      = impl_avr_timer_set_ocra;
    api.avr_timer_get_ocrb      = impl_avr_timer_get_ocrb;
    api.avr_timer_set_ocrb      = impl_avr_timer_set_ocrb;
    return api;
}

void ArduinoRuntime::impl_pinMode(int pin, int mode) {
    if (!g_runtime || pin < 0 || pin >= g_runtime->profile_.pin_count) return;
    g_runtime->state_.pin_modes[pin] = mode;
    if (mode == 2)
        g_runtime->state_.pin_values[pin] = 1;
}

void ArduinoRuntime::impl_digitalWrite(int pin, int value) {
    if (!g_runtime || pin < 0 || pin >= g_runtime->profile_.pin_count) return;
    int  old_value = g_runtime->state_.pin_values[pin];
    bool changed   = (old_value != value);
    g_runtime->state_.pin_values[pin] = value;
    if (!changed) return;
    if (g_runtime->on_pin_changed)
        g_runtime->on_pin_changed(pin, value);
    else
        std::cout << ts() << "  pin " << pin
                  << " -> " << (value ? "HIGH" : "LOW") << "\n";

    if (!g_runtime->state_.interrupts_enabled_) return;

    // Dispatch attachInterrupt callbacks (registered via attachInterrupt())
    {
        auto cb_it = g_runtime->state_.interrupt_callbacks_.find(pin);
        if (cb_it != g_runtime->state_.interrupt_callbacks_.end() && cb_it->second) {
            int mode = g_runtime->state_.interrupt_modes_[pin];
            bool fire = (mode == vb::CHANGE) ||
                        (mode == vb::RISING  && old_value == 0 && value == 1) ||
                        (mode == vb::FALLING && old_value == 1 && value == 0);
            if (fire) {
                g_runtime->state_.interrupts_enabled_ = false;
                cb_it->second();
                g_runtime->state_.interrupts_enabled_ = true;
            }
        }
    }

    // Dispatch ISR vector handlers (registered via ISR() macro transform)
    auto dispatch_vec = [&](const char* vect_name) {
        auto it = g_runtime->state_.isr_handlers_.find(vect_name);
        if (it == g_runtime->state_.isr_handlers_.end() || !it->second) return;
        g_runtime->state_.interrupts_enabled_ = false;
        it->second();
        g_runtime->state_.interrupts_enabled_ = true;
    };

    // External interrupts: INT0=pin2, INT1=pin3 (Uno/Nano/Mega mapping)
    if (pin == 2) dispatch_vec("INT0_vect");
    if (pin == 3) dispatch_vec("INT1_vect");
    // Pin-change interrupt groups (Uno port mapping)
    if (pin >= 0  && pin <= 7)  dispatch_vec("PCINT2_vect"); // port D
    if (pin >= 8  && pin <= 13) dispatch_vec("PCINT0_vect"); // port B
    if (pin >= 14 && pin <= 19) dispatch_vec("PCINT1_vect"); // port C (A0-A5)

    if (g_runtime->state_.sleep_enabled_) {
        std::lock_guard<std::mutex> lock(g_runtime->state_.sleep_mtx_);
        g_runtime->state_.sleep_woken_ = true;
        g_runtime->state_.sleep_cv_.notify_all();
    }
}

int ArduinoRuntime::impl_digitalRead(int pin) {
    if (!g_runtime || pin < 0 || pin >= g_runtime->profile_.pin_count) return 0;
    // Button bounce: return random during the bounce window, then settle
    auto bounce_it = g_runtime->state_.pin_bounce_until_.find(pin);
    if (bounce_it != g_runtime->state_.pin_bounce_until_.end()) {
        if (std::chrono::steady_clock::now() < bounce_it->second)
            return g_runtime->rng_() & 1;
        int settled = g_runtime->state_.pin_bounce_target[pin];
        g_runtime->state_.pin_values[pin] = settled;
        g_runtime->state_.pin_bounce_until_.erase(bounce_it);
        return settled;
    }
    // Keypad matrix: this column's electrical value depends on which row is
    // currently driven active by the injected Keypad class's own scan loop --
    // pulled high (idle) unless the actively-driven row's key is held.
    {
        std::lock_guard<std::mutex> lock(g_runtime->state_.keypad_mtx_);
        auto it = g_runtime->state_.keypad_col_rows_.find(pin);
        if (it != g_runtime->state_.keypad_col_rows_.end()) {
            for (int row_pin : it->second) {
                if (g_runtime->state_.pin_values[row_pin] == 0 &&
                    g_runtime->state_.keypad_pressed_.count({row_pin, pin}))
                    return 0;
            }
            return 1;
        }
    }
    // Floating pin: INPUT mode (0) with nothing injected from UI returns random
    if (g_runtime->state_.pin_modes[pin] == 0 && !g_runtime->state_.pin_driven[pin])
        return g_runtime->rng_() & 1;
    return g_runtime->state_.pin_values[pin];
}

void ArduinoRuntime::impl_analogWrite(int pin, int value) {
    if (!g_runtime || pin < 0 || pin >= g_runtime->profile_.pin_count) return;
    bool changed = (g_runtime->state_.pwm_values[pin] != value);
    g_runtime->state_.pwm_values[pin] = value;
    if (!changed) return;
    if (g_runtime->on_pin_changed)
        g_runtime->on_pin_changed(pin, value);
}


int ArduinoRuntime::impl_analogRead(int pin) {
    if (!g_runtime) return 0;
    int analog_index = (pin >= g_runtime->profile_.analog_offset) ? pin - g_runtime->profile_.analog_offset : pin;
    if (analog_index < 0 || analog_index >= g_runtime->profile_.analog_count) return 0;
    int value = g_runtime->state_.analog_values[analog_index];
    if (g_runtime->analog_noise_enabled_) {
        std::normal_distribution<float> noise(0.0f, 2.0f);
        value += (int)std::round(noise(g_runtime->rng_));
        value = std::max(0, std::min(1023, value));
    }
    return value;
}

void ArduinoRuntime::impl_delay(unsigned long ms) {
    if (!g_runtime) return;
    unsigned long scaled = (unsigned long)(ms * g_runtime->speed_multiplier_);

    // Sleep in 10ms chunks so stop requests are handled quickly, releasing
    // exec_mtx_ each chunk so ISR handlers can still preempt a delay() call,
    // same as real interrupts firing during a busy-wait.
    unsigned long elapsed = 0;
    while (elapsed < scaled) {
        if (g_runtime->stop_requested_) return;
        unsigned long chunk = qMin(10UL, scaled - elapsed);
        g_runtime->state_.exec_mtx_.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        g_runtime->state_.exec_mtx_.lock();
        elapsed += chunk;
    }
}

unsigned long ArduinoRuntime::impl_millis() {
    if (!g_runtime) return 0;
    auto real_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - g_runtime->state_.start_time).count();
    return (unsigned long)(real_us / g_runtime->speed_multiplier_ / 1000);
}

unsigned long ArduinoRuntime::impl_micros() {
    if (!g_runtime) return 0;
    auto real_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - g_runtime->state_.start_time).count();
    return (unsigned long)(real_us / g_runtime->speed_multiplier_);
}

void ArduinoRuntime::impl_Serial_begin(int baud) {
    if (!g_runtime) return;
    g_runtime->state_.serial_started = true;
    g_runtime->state_.serial_baud    = baud;
}

void ArduinoRuntime::impl_Serial_print(const char* s) {
    if (g_runtime && g_runtime->on_serial_output)
        g_runtime->on_serial_output(std::string(s));
    else
        std::cout << ts() << "  Serial >> " << s;
}

void ArduinoRuntime::impl_Serial_println(const char* s) {
    if (g_runtime && g_runtime->on_serial_output)
        g_runtime->on_serial_output(std::string(s) + "\n");
    else
        std::cout << ts() << "  Serial >> " << s << "\n";
}

void ArduinoRuntime::inject_pin(int pin, int value) {
    if (pin < 0 || pin >= profile_.pin_count) return;
    g_runtime = this;
    state_.pin_driven[pin] = true;
    impl_digitalWrite(pin, value);
}

void ArduinoRuntime::inject_button_bounce(int pin, int finalValue) {
    if (pin < 0 || pin >= profile_.pin_count) return;
    g_runtime = this;
    state_.pin_driven[pin] = true;
    state_.pin_bounce_target[pin] = finalValue;
    state_.pin_bounce_until_[pin] = std::chrono::steady_clock::now()
                                    + std::chrono::milliseconds(10);
    impl_digitalWrite(pin, finalValue); // dispatch ISRs on the settled edge
}

void ArduinoRuntime::impl_watch_variable(const char* name, int value) {
    if (g_runtime && g_runtime->on_variable_changed)
        g_runtime->on_variable_changed(std::string(name), value);
    else
        std::cout << ts() << "  watch: " << name << " = " << value << "\n";
}

void ArduinoRuntime::impl_lcd_print(int pin, int row, const char* text) {
    if (g_runtime && g_runtime->on_lcd_print)
        g_runtime->on_lcd_print(pin, row, std::string(text ? text : ""));
}

void ArduinoRuntime::set_speed_multiplier(float speed) {
    speed_multiplier_ = 1.0f / speed;
}

int ArduinoRuntime::impl_Serial_available() {
    if (!g_runtime) return 0;
    return (int)g_runtime->serial_buffer_.size();
}

int ArduinoRuntime::impl_Serial_read() {
    if (!g_runtime || g_runtime->serial_buffer_.empty()) return -1;
    char c = g_runtime->serial_buffer_.front();
    g_runtime->serial_buffer_.pop_front();
    return (int)c;
}

unsigned long ArduinoRuntime::impl_pulseIn(int pin, int value, unsigned long timeout) {
    if (!g_runtime) return 0;

    if (pin >= 0 && pin < g_runtime->profile_.pin_count && g_runtime->state_.pulse_durations_[pin] != 0) {
        return g_runtime->state_.pulse_durations_[pin];
    }

    {
        std::lock_guard<std::mutex> lock(g_runtime->state_.color_mtx_);
        if (g_runtime->state_.color_channels_.count(pin)) {
            int s2_pin = g_runtime->state_.color_sensor_s2_[pin];
            int s3_pin = g_runtime->state_.color_sensor_s3_[pin];
            auto channels = g_runtime->state_.color_channels_[pin];
            int s2 = impl_digitalRead(s2_pin);
            int s3 = impl_digitalRead(s3_pin);
            return channels[s2 * 2 + s3];
        }
    }
    unsigned long start_time = impl_micros();
    // Phase 1
    while (impl_digitalRead(pin) == value) {
        if (g_runtime->stop_requested_) return 0;
        if (impl_micros() - start_time >= timeout) return 0;
        g_runtime->state_.exec_mtx_.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        g_runtime->state_.exec_mtx_.lock();
    }
    // Phase 2
    while (impl_digitalRead(pin) != value) {
        if (g_runtime->stop_requested_) return 0;
        if (impl_micros() - start_time >= timeout) return 0;
        g_runtime->state_.exec_mtx_.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        g_runtime->state_.exec_mtx_.lock();
    }
    unsigned long pulse_start = impl_micros();
    // Phase 3
    while (impl_digitalRead(pin) == value) {
        if (g_runtime->stop_requested_) return 0;
        if (impl_micros() - start_time >= timeout) return 0;
        g_runtime->state_.exec_mtx_.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        g_runtime->state_.exec_mtx_.lock();
    }
    unsigned long pulse_end = impl_micros();
    return pulse_end - pulse_start;
}

void ArduinoRuntime::impl_delayMicroseconds(unsigned long us) {
    if (!g_runtime) return;
    unsigned long scaled = (unsigned long)(us * g_runtime->speed_multiplier_);
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - start).count() < scaled) {
        if (g_runtime->stop_requested_) return;
        g_runtime->state_.exec_mtx_.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        g_runtime->state_.exec_mtx_.lock();
    }
}

void ArduinoRuntime::impl_tone(int pin, int frequency, int duration_ms) {
    if (!g_runtime) return;
    g_runtime->state_.tone_frequencies_[pin] = frequency;
    if (g_runtime->on_pin_changed) g_runtime->on_pin_changed(pin, frequency);
    if (duration_ms == 0) return;
    ArduinoRuntime* rt = g_runtime;
    std::thread t([pin, duration_ms, rt]() {
        unsigned long scaled_duration = (unsigned long)(duration_ms * rt->speed_multiplier_);
        unsigned long elapsed = 0;
        while (elapsed < scaled_duration) {
            if (rt->stop_requested_) return;
            unsigned long chunk = std::min(10UL, scaled_duration - elapsed);
            std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
            elapsed += chunk;
        }
        if (!rt->stop_requested_) {
            rt->state_.tone_frequencies_[pin] = 0;
            if (rt->on_pin_changed) rt->on_pin_changed(pin, 0);
        }
    });
    std::lock_guard<std::mutex> lock(rt->tone_threads_mtx_);
    rt->tone_threads_.push_back(std::move(t));
}

void ArduinoRuntime::impl_noTone(int pin) {
    if (!g_runtime) return;
    g_runtime->state_.tone_frequencies_[pin] = 0;
    if (g_runtime->on_pin_changed) g_runtime->on_pin_changed(pin, 0);
}

void ArduinoRuntime::impl_attachInterrupt(int pin, void (*callback)(), int mode) {
    if (!g_runtime) return;
    g_runtime->state_.interrupt_callbacks_[pin] = callback;
    g_runtime->state_.interrupt_modes_[pin] = mode;
}

void ArduinoRuntime::impl_register_isr(const char* vector_name, void (*handler)()) {
    if (!g_runtime || !vector_name || !handler) return;
    g_runtime->state_.isr_handlers_[vector_name] = handler;
}

void ArduinoRuntime::impl_noInterrupts() {
    if (!g_runtime) return;
    g_runtime->state_.interrupts_enabled_ = false;
}

void ArduinoRuntime::impl_interrupts() {
    if (!g_runtime) return;
    g_runtime->state_.interrupts_enabled_ = true;
}

void ArduinoRuntime::impl_EEPROM_write(int address, uint8_t value) {
    if (!g_runtime || address < 0 || address >= 1024) return;
    g_runtime->state_.eeprom_[address] = value;
}

uint8_t ArduinoRuntime::impl_EEPROM_read(int address) {
    if (!g_runtime || address < 0 || address >= 1024) return 0xFF;
    return g_runtime->state_.eeprom_[address];
}

void ArduinoRuntime::impl_EEPROM_update(int address, uint8_t value) {
    if (!g_runtime || address < 0 || address >= 1024) return;
    if (g_runtime->state_.eeprom_[address] != value)
        g_runtime->state_.eeprom_[address] = value;
}

void ArduinoRuntime::impl_Serial1_begin(int baud) {
    if (!g_runtime) return;
    g_runtime->state_.serial1_started = true;
    g_runtime->state_.serial1_baud_ = baud;
}

void ArduinoRuntime::impl_Serial1_print(const char* s) {
    if (g_runtime && g_runtime->on_serial1_output)
        g_runtime->on_serial1_output(std::string(s));
    else
        std::cout << ts() << "  Serial[1] >> " << s;
}

void ArduinoRuntime::impl_Serial1_println(const char* s) {
    if (g_runtime && g_runtime->on_serial1_output)
        g_runtime->on_serial1_output(std::string(s) + "\n");
    else
        std::cout << ts() << "  Serial[1] >> " << s << "\n";
}

void ArduinoRuntime::impl_Serial2_begin(int baud) {
    if (!g_runtime) return;
    g_runtime->state_.serial2_started = true;
    g_runtime->state_.serial2_baud_ = baud;
}

void ArduinoRuntime::impl_Serial2_print(const char* s) {
    if (g_runtime && g_runtime->on_serial2_output)
        g_runtime->on_serial2_output(std::string(s));
    else
        std::cout << ts() << "  Serial[2] >> " << s;
}

void ArduinoRuntime::impl_Serial2_println(const char* s) {
    if (g_runtime && g_runtime->on_serial2_output)
        g_runtime->on_serial2_output(std::string(s) + "\n");
    else
        std::cout << ts() << "  Serial[2] >> " << s << "\n";
}

void ArduinoRuntime::impl_soft_serial_begin(int rxPin, int baud) {
    if (!g_runtime) return;
    (void)baud;
    g_runtime->state_.soft_serial_buffers_[rxPin]; // ensure buffer entry exists
}

void ArduinoRuntime::impl_soft_serial_print(int rxPin, const char* s) {
    if (!g_runtime) return;
    if (g_runtime->on_soft_serial_output)
        g_runtime->on_soft_serial_output(rxPin, std::string(s));
    else
        std::cout << ts() << "  SW:" << rxPin << " >> " << s;
}

void ArduinoRuntime::impl_soft_serial_println(int rxPin, const char* s) {
    if (!g_runtime) return;
    if (g_runtime->on_soft_serial_output)
        g_runtime->on_soft_serial_output(rxPin, std::string(s) + "\n");
    else
        std::cout << ts() << "  SW:" << rxPin << " >> " << s << "\n";
}

int ArduinoRuntime::impl_soft_serial_available(int rxPin) {
    if (!g_runtime) return 0;
    auto it = g_runtime->state_.soft_serial_buffers_.find(rxPin);
    if (it == g_runtime->state_.soft_serial_buffers_.end()) return 0;
    return (int)it->second.size();
}

int ArduinoRuntime::impl_soft_serial_read(int rxPin) {
    if (!g_runtime) return -1;
    auto it = g_runtime->state_.soft_serial_buffers_.find(rxPin);
    if (it == g_runtime->state_.soft_serial_buffers_.end() || it->second.empty()) return -1;
    char c = it->second.front();
    it->second.pop_front();
    return (int)(unsigned char)c;
}

int ArduinoRuntime::impl_soft_serial_peek(int rxPin) {
    if (!g_runtime) return -1;
    auto it = g_runtime->state_.soft_serial_buffers_.find(rxPin);
    if (it == g_runtime->state_.soft_serial_buffers_.end() || it->second.empty()) return -1;
    return (int)(unsigned char)it->second.front();
}

void ArduinoRuntime::impl_wdt_reset(){
    if(!g_runtime) return;

    std::lock_guard<std::mutex> lock(g_runtime->state_.sleep_mtx_);
    g_runtime->state_.wdt_last_reset_ = std::chrono::steady_clock::now();
}

void ArduinoRuntime::impl_wdt_disable(){
    if (!g_runtime) return;

    std::lock_guard<std::mutex> lock(g_runtime->state_.sleep_mtx_);
    g_runtime->state_.wdt_enabled_ = false;
}

void ArduinoRuntime::impl_wdt_enable(int timeout_ms) {
    if (!g_runtime) return;
    ArduinoRuntime* rt = g_runtime; // capture before thread — g_runtime is thread_local

    // Join any previous watchdog monitor before starting a new one, and
    // before touching state_ again -- see stop_wdt_thread()'s comment.
    rt->stop_wdt_thread();

    {
        std::lock_guard<std::mutex> lock(rt->state_.sleep_mtx_);
        rt->state_.wdt_enabled_    = true;
        rt->state_.wdt_timeout_ms_ = timeout_ms;
        rt->state_.wdt_last_reset_ = std::chrono::steady_clock::now(); // start the clock
    }

    rt->wdt_thread_stop_ = false;
    rt->wdt_thread_ = std::thread([rt]() {
        g_runtime = rt;
        while (!rt->wdt_thread_stop_ && !rt->stop_requested_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            bool timed_out;
            {
                std::lock_guard<std::mutex> lock(rt->state_.sleep_mtx_);
                if (!rt->state_.wdt_enabled_) return; // disabled from the sketch thread
                auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - rt->state_.wdt_last_reset_).count();
                timed_out = age_ms >= rt->state_.wdt_timeout_ms_;
            }
            if (!timed_out) continue;

            bool sleeping;
            {
                std::lock_guard<std::mutex> lock(rt->state_.sleep_mtx_);
                sleeping = rt->state_.sleep_enabled_;
                if (sleeping) rt->state_.sleep_woken_ = true;
            }
            if (sleeping) rt->state_.sleep_cv_.notify_all();

            auto isr_it = rt->state_.isr_handlers_.find("WDT_vect");
            if (isr_it != rt->state_.isr_handlers_.end() && isr_it->second) {
                // Interrupt mode: fire ISR, reset timer, keep watching
                rt->state_.exec_mtx_.lock();
                rt->state_.interrupts_enabled_ = false;
                isr_it->second();
                rt->state_.interrupts_enabled_ = true;
                rt->state_.exec_mtx_.unlock();
                std::lock_guard<std::mutex> lock(rt->state_.sleep_mtx_);
                rt->state_.wdt_last_reset_ = std::chrono::steady_clock::now();
            } else {
                if (rt->on_watchdog_reset) rt->on_watchdog_reset();
                return;
            }
        }
    });
}

void ArduinoRuntime::impl_set_sleep_mode(int mode) {
    if (!g_runtime) return;
    g_runtime->state_.sleep_mode_ = mode;
}

void ArduinoRuntime::impl_sleep_enable() {
    if (!g_runtime) return;
    std::lock_guard<std::mutex> lock(g_runtime->state_.sleep_mtx_);
    g_runtime->state_.sleep_enabled_ = true;
}

void ArduinoRuntime::impl_sleep_disable() {
    if (!g_runtime) return;
    std::lock_guard<std::mutex> lock(g_runtime->state_.sleep_mtx_);
    g_runtime->state_.sleep_enabled_ = false;
}

void ArduinoRuntime::impl_sleep_cpu() {
    if (!g_runtime || !g_runtime->state_.sleep_enabled_) return;

    if (g_runtime->on_sleep_changed) g_runtime->on_sleep_changed(true);

    // Release exec_mtx_ for the wait -- otherwise the watchdog thread could
    // never acquire it to fire the WDT ISR that's supposed to wake us up.
    g_runtime->state_.exec_mtx_.unlock();
    {
        std::unique_lock<std::mutex> lock(g_runtime->state_.sleep_mtx_);
        g_runtime->state_.sleep_woken_ = false;
        g_runtime->state_.sleep_cv_.wait(lock, [] {
            return g_runtime->state_.sleep_woken_ || g_runtime->stop_requested_.load();
        });
    }
    g_runtime->state_.exec_mtx_.lock();

    if (g_runtime->on_sleep_changed) g_runtime->on_sleep_changed(false);
}

void ArduinoRuntime::impl_wire_begin_transmission(int address) {
    if (!g_runtime) return;
    g_runtime->state_.wire_tx_address_ = address;
    g_runtime->state_.wire_tx_buffer_.clear();
}

void ArduinoRuntime::impl_wire_write(uint8_t b) {
    if (!g_runtime) return;
    g_runtime->state_.wire_tx_buffer_.push_back(b);
}

int ArduinoRuntime::impl_wire_end_transmission() {
    if (!g_runtime) return 4; // "other error" -- matches real Wire's status code for no active transmission
    g_runtime->state_.wire_tx_address_ = -1;
    g_runtime->state_.wire_tx_buffer_.clear();
    return 0; // success -- no bus errors are modeled
}

int ArduinoRuntime::impl_wire_request_from(int address, int quantity) {
    if (!g_runtime || quantity <= 0) return 0;
    std::vector<uint8_t> bytes;
    {
        std::lock_guard<std::mutex> lock(g_runtime->state_.wire_mtx_);
        auto it = g_runtime->state_.wire_devices_.find(address);
        if (it != g_runtime->state_.wire_devices_.end())
            bytes = it->second;
    }
    g_runtime->state_.wire_rx_buffer_.clear();
    for (int i = 0; i < quantity; ++i)
        g_runtime->state_.wire_rx_buffer_.push_back(i < (int)bytes.size() ? bytes[i] : 0);
    return (int)g_runtime->state_.wire_rx_buffer_.size();
}

int ArduinoRuntime::impl_wire_available() {
    if (!g_runtime) return 0;
    return (int)g_runtime->state_.wire_rx_buffer_.size();
}

int ArduinoRuntime::impl_wire_read() {
    if (!g_runtime || g_runtime->state_.wire_rx_buffer_.empty()) return -1;
    uint8_t b = g_runtime->state_.wire_rx_buffer_.front();
    g_runtime->state_.wire_rx_buffer_.pop_front();
    return b;
}

uint8_t ArduinoRuntime::impl_spi_transfer(uint8_t /*tx*/) {
    if (!g_runtime) return 0;
    std::lock_guard<std::mutex> lock(g_runtime->state_.spi_mtx_);
    auto& bytes = g_runtime->state_.spi_response_bytes_;
    if (bytes.empty()) return 0;
    uint8_t b = bytes[g_runtime->state_.spi_response_index_];
    g_runtime->state_.spi_response_index_ = (g_runtime->state_.spi_response_index_ + 1) % bytes.size();
    return b;
}

AvrTimerState& ArduinoRuntime::timer_state(ArduinoRuntime* rt, int timerId) {
    return timerId == 1 ? rt->state_.timer1_ : rt->state_.timer2_;
}

long long ArduinoRuntime::avr_scaled_micros(ArduinoRuntime* rt) {
    auto real_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - rt->state_.start_time).count();
    return (long long)(real_us / rt->speed_multiplier_);
}

uint8_t ArduinoRuntime::impl_avr_timer_get_tccra(int timerId) {
    if (!g_runtime) return 0;
    std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
    return timer_state(g_runtime, timerId).tccra;
}

void ArduinoRuntime::impl_avr_timer_set_tccra(int timerId, uint8_t value) {
    if (!g_runtime) return;
    std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
    timer_state(g_runtime, timerId).tccra = value;
}

uint8_t ArduinoRuntime::impl_avr_timer_get_tccrb(int timerId) {
    if (!g_runtime) return 0;
    std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
    return timer_state(g_runtime, timerId).tccrb;
}

void ArduinoRuntime::impl_avr_timer_set_tccrb(int timerId, uint8_t value) {
    if (!g_runtime) return;
    AvrTimerState& st = timer_state(g_runtime, timerId);
    long long avr_us = avr_scaled_micros(g_runtime);
    {
        // Rebase so time elapsed at the old prescaler is preserved before
        // switching to the new one.
        std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
        long long raw = avr_raw_ticks(st, avr_us);
        st.ref_ticks = raw;
        st.ref_avr_us = avr_us;
        st.last_check_raw = raw;
        st.tccrb = value;
        st.prescaler = (timerId == 1) ? avr_timer1_prescaler(value) : avr_timer2_prescaler(value);
    }
    g_runtime->ensure_timer_thread_running();
}

uint8_t ArduinoRuntime::impl_avr_timer_get_timsk(int timerId) {
    if (!g_runtime) return 0;
    std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
    return timer_state(g_runtime, timerId).timsk;
}

void ArduinoRuntime::impl_avr_timer_set_timsk(int timerId, uint8_t value) {
    if (!g_runtime) return;
    {
        std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
        timer_state(g_runtime, timerId).timsk = value;
    }
    g_runtime->ensure_timer_thread_running();
}

uint32_t ArduinoRuntime::impl_avr_timer_get_tcnt(int timerId) {
    if (!g_runtime) return 0;
    AvrTimerState& st = timer_state(g_runtime, timerId);
    long long avr_us = avr_scaled_micros(g_runtime);
    std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
    long long raw = avr_raw_ticks(st, avr_us);
    return (uint32_t)(((raw % st.modulus) + st.modulus) % st.modulus);
}

void ArduinoRuntime::impl_avr_timer_set_tcnt(int timerId, uint32_t value) {
    if (!g_runtime) return;
    AvrTimerState& st = timer_state(g_runtime, timerId);
    long long avr_us = avr_scaled_micros(g_runtime);
    std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
    st.ref_ticks = (long long)(value % (uint32_t)st.modulus);
    st.ref_avr_us = avr_us;
    st.last_check_raw = st.ref_ticks;
}

uint32_t ArduinoRuntime::impl_avr_timer_get_ocra(int timerId) {
    if (!g_runtime) return 0;
    std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
    return timer_state(g_runtime, timerId).ocra;
}

void ArduinoRuntime::impl_avr_timer_set_ocra(int timerId, uint32_t value) {
    if (!g_runtime) return;
    AvrTimerState& st = timer_state(g_runtime, timerId);
    int pin;
    uint32_t duty;
    {
        std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
        st.ocra = value % (uint32_t)st.modulus;
        pin = st.pinA;
        duty = std::min<uint32_t>(st.ocra, 255);
    }
    if (pin >= 0) impl_analogWrite(pin, (int)duty);
}

uint32_t ArduinoRuntime::impl_avr_timer_get_ocrb(int timerId) {
    if (!g_runtime) return 0;
    std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
    return timer_state(g_runtime, timerId).ocrb;
}

void ArduinoRuntime::impl_avr_timer_set_ocrb(int timerId, uint32_t value) {
    if (!g_runtime) return;
    AvrTimerState& st = timer_state(g_runtime, timerId);
    int pin;
    uint32_t duty;
    {
        std::lock_guard<std::mutex> lock(g_runtime->state_.timer_mtx_);
        st.ocrb = value % (uint32_t)st.modulus;
        pin = st.pinB;
        duty = std::min<uint32_t>(st.ocrb, 255);
    }
    if (pin >= 0) impl_analogWrite(pin, (int)duty);
}

void ArduinoRuntime::dispatch_isr(const char* vect_name) {
    if (!state_.interrupts_enabled_) return;
    auto it = state_.isr_handlers_.find(vect_name);
    if (it == state_.isr_handlers_.end() || !it->second) return;
    state_.exec_mtx_.lock();
    state_.interrupts_enabled_ = false;
    it->second();
    state_.interrupts_enabled_ = true;
    state_.exec_mtx_.unlock();
}

void ArduinoRuntime::poll_timer(AvrTimerState& st, long long avr_us,
                                const char* ovf_vect, const char* compa_vect, const char* compb_vect,
                                int toie_bit, int ociea_bit, int ocieb_bit) {
    long long ovf_count, compa_count, compb_count;
    bool ovf_en, compa_en, compb_en;
    {
        std::lock_guard<std::mutex> lock(state_.timer_mtx_);
        if (st.prescaler <= 0) return;
        long long raw = avr_raw_ticks(st, avr_us);
        long long prev = st.last_check_raw;
        st.last_check_raw = raw;
        ovf_count   = avr_count_crossings(prev, raw, 0, st.modulus);
        compa_count = avr_count_crossings(prev, raw, (long long)(st.ocra % (uint32_t)st.modulus), st.modulus);
        compb_count = avr_count_crossings(prev, raw, (long long)(st.ocrb % (uint32_t)st.modulus), st.modulus);
        ovf_en   = st.timsk & (1 << toie_bit);
        compa_en = st.timsk & (1 << ociea_bit);
        compb_en = st.timsk & (1 << ocieb_bit);
    }
    if (ovf_en)   for (long long i = 0; i < ovf_count; ++i)   dispatch_isr(ovf_vect);
    if (compa_en) for (long long i = 0; i < compa_count; ++i) dispatch_isr(compa_vect);
    if (compb_en) for (long long i = 0; i < compb_count; ++i) dispatch_isr(compb_vect);
}

void ArduinoRuntime::ensure_timer_thread_running() {
    bool expected = false;
    if (!timer_thread_started_.compare_exchange_strong(expected, true)) return;
    ArduinoRuntime* rt = this;
    timer_thread_stop_ = false;
    timer_thread_ = std::thread([rt]() {
        g_runtime = rt;
        while (!rt->timer_thread_stop_ && !rt->stop_requested_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            long long avr_us = avr_scaled_micros(rt);
            rt->poll_timer(rt->state_.timer1_, avr_us,
                           "TIMER1_OVF_vect", "TIMER1_COMPA_vect", "TIMER1_COMPB_vect", 0, 1, 2);
            rt->poll_timer(rt->state_.timer2_, avr_us,
                           "TIMER2_OVF_vect", "TIMER2_COMPA_vect", "TIMER2_COMPB_vect", 0, 1, 2);
        }
    });
}

void ArduinoRuntime::stop_timer_thread() {
    timer_thread_stop_ = true;
    if (timer_thread_.joinable())
        timer_thread_.join();
    timer_thread_started_ = false;
}

void ArduinoRuntime::stop_tone_threads() {
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lock(tone_threads_mtx_);
        threads.swap(tone_threads_);
    }
    for (auto& t : threads)
        if (t.joinable()) t.join();
}