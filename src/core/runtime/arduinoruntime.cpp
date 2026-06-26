#include "src/core/runtime/arduinoruntime.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
//#include <algorithm>
#include <QtGlobal>

thread_local ArduinoRuntime* g_runtime = nullptr;

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

    // Sleep in 10ms chunks so stop requests are handled quickly
    unsigned long elapsed = 0;
    while (elapsed < scaled) {
        if (g_runtime->stop_requested_) return;
        unsigned long chunk = qMin(10UL, scaled - elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
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

    if (g_runtime->state_.color_channels_.count(pin)) {
        int s2 = impl_digitalRead(g_runtime->state_.color_sensor_s2_[pin]);
        int s3 = impl_digitalRead(g_runtime->state_.color_sensor_s3_[pin]);
        return g_runtime->state_.color_channels_[pin][s2 * 2 + s3];
    }
    unsigned long start_time = impl_micros();
    // Phase 1
    while (impl_digitalRead(pin) == value) {
        if (g_runtime->stop_requested_) return 0;
        if (impl_micros() - start_time >= timeout) return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // Phase 2
    while (impl_digitalRead(pin) != value) {
        if (g_runtime->stop_requested_) return 0;
        if (impl_micros() - start_time >= timeout) return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    unsigned long pulse_start = impl_micros();
    // Phase 3
    while (impl_digitalRead(pin) == value) {
        if (g_runtime->stop_requested_) return 0;
        if (impl_micros() - start_time >= timeout) return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

void ArduinoRuntime::impl_tone(int pin, int frequency, int duration_ms) {
    if (!g_runtime) return;
    g_runtime->state_.tone_frequencies_[pin] = frequency;
    if (g_runtime->on_pin_changed) g_runtime->on_pin_changed(pin, frequency);
    if (duration_ms == 0) return;
    ArduinoRuntime* rt = g_runtime;
    std::thread([pin, duration_ms, rt]() {
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
    }).detach();
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

    g_runtime->state_.wdt_last_reset_ = std::chrono::steady_clock::now();
}

void ArduinoRuntime::impl_wdt_disable(){
    if (!g_runtime) return;

    g_runtime->state_.wdt_enabled_ = false;
}

void ArduinoRuntime::impl_wdt_enable(int timeout_ms) {
    if (!g_runtime) return;
    g_runtime->state_.wdt_enabled_   = true;
    g_runtime->state_.wdt_timeout_ms_ = timeout_ms;
    g_runtime->state_.wdt_last_reset_ = std::chrono::steady_clock::now(); // start the clock

    ArduinoRuntime* rt = g_runtime; // capture before thread — g_runtime is thread_local
    std::thread([rt]() {
        while (rt->state_.wdt_enabled_ && !rt->stop_requested_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - rt->state_.wdt_last_reset_).count();
            if (age_ms >= rt->state_.wdt_timeout_ms_) {
                if (rt->on_watchdog_reset) rt->on_watchdog_reset();
                return;
            }
        }
    }).detach();
}