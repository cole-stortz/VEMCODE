#include "src/core/runtime/arduinoruntime.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <QtGlobal>

static ArduinoRuntime* g_runtime = nullptr;

static std::string ts() {
    if (!g_runtime) return "[?ms] ";
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() -
        g_runtime->get_state().start_time).count();
    std::ostringstream ss;
    ss << "[" << std::setw(8) << us / 1000 << "ms] ";
    return ss.str();
}

ArduinoRuntime::ArduinoRuntime() {
    state_.start_time = std::chrono::steady_clock::now();
}

ArduinoAPI ArduinoRuntime::get_api() {
    g_runtime = this;
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
    return api;
}

void ArduinoRuntime::impl_pinMode(int pin, int mode) {
    if (!g_runtime || pin < 0 || pin >= 20) return;
    g_runtime->state_.pin_modes[pin] = mode;
    if (mode == 2)
        g_runtime->state_.pin_values[pin] = 1;
}

void ArduinoRuntime::impl_digitalWrite(int pin, int value) {
    if (!g_runtime || pin < 0 || pin >= 20) return; // return if pins are outside the arduino range
    bool changed = (g_runtime->state_.pin_values[pin] != value); // Check to see if the value has changed
    g_runtime->state_.pin_values[pin] = value; // Set the value
    if (!changed) return; // If didnt change return
    if (g_runtime->on_pin_changed) 
        g_runtime->on_pin_changed(pin, value); // Set the changed pin value
    else
        std::cout << ts() << "  pin " << pin
                  << " -> " << (value ? "HIGH" : "LOW") << "\n";
}

int ArduinoRuntime::impl_digitalRead(int pin) {
    if (!g_runtime || pin < 0 || pin >= 20) return 0; // return if digital pins are outside the arduino range
    return g_runtime->state_.pin_values[pin]; // Return the pin value
}

// TODO: add on_analog_changed callback when potentiometer component is implemented
void ArduinoRuntime::impl_analogWrite(int pin, int value) {
    std::cout << ts() << "  analogWrite(" << pin << ", " << value << ")\n";
}

int ArduinoRuntime::impl_analogRead(int pin) {
    if (!g_runtime) return 0;
    int analog_index = (pin >= 14) ? pin - 14 : pin;
    if (analog_index < 0 || analog_index >= 8) return 0;
    return g_runtime->state_.analog_values[analog_index];
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
    if (!g_runtime) return 0; // if not running return
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() -
        g_runtime->state_.start_time).count(); // start millisecond timer
    return (unsigned long)ms; // return time
}

unsigned long ArduinoRuntime::impl_micros() {
    if (!g_runtime) return 0; // if not running return
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() -
        g_runtime->state_.start_time).count(); // start microsecond timer
    return (unsigned long)us; // return time
}

void ArduinoRuntime::impl_Serial_begin(int baud) {
    if (!g_runtime) return; // if not running return
    g_runtime->state_.serial_started = true; // simple set values
    g_runtime->state_.serial_baud    = baud;
    //std::cout << ts() << "  Serial.begin(" << baud << ")\n";
}

void ArduinoRuntime::impl_Serial_print(const char* s) {
    if (g_runtime && g_runtime->on_serial_output) // send output to device
        g_runtime->on_serial_output(std::string(s));
    else
        std::cout << ts() << "  Serial >> " << s; // output to console
}

void ArduinoRuntime::impl_Serial_println(const char* s) {
    if (g_runtime && g_runtime->on_serial_output) // send output to device
        g_runtime->on_serial_output(std::string(s) + "\n");
    else
        std::cout << ts() << "  Serial >> " << s << "\n"; // send to console
}

void ArduinoRuntime::inject_pin(int pin, int value) {
    if (pin >= 0 && pin < 20) 
        state_.pin_values[pin] = value; // set pin state if in range
}

void ArduinoRuntime::impl_watch_variable(const char* name, int value) {
    if (g_runtime && g_runtime->on_variable_changed)
        g_runtime->on_variable_changed(std::string(name), value);
    else
        std::cout << ts() << "  watch: " << name << " = " << value << "\n";
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

    if (pin >= 0 && pin < 20 && g_runtime->state_.pulse_durations_[pin] != 0) {
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
