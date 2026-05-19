#include "src/core/runtime/arduinoruntime.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

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
    return api;
}

void ArduinoRuntime::impl_pinMode(int pin, int mode) {
    if (!g_runtime || pin < 0 || pin >= 20) return;
    g_runtime->state_.pin_modes[pin] = mode;
    const char* mode_str = (mode == 1) ? "OUTPUT" :
                           (mode == 2) ? "INPUT_PULLUP" : "INPUT";
    std::cout << ts() << "  pinMode(" << pin << ", " << mode_str << ")\n";
}

void ArduinoRuntime::impl_digitalWrite(int pin, int value) {
    if (!g_runtime || pin < 0 || pin >= 20) return;
    bool changed = (g_runtime->state_.pin_values[pin] != value);
    g_runtime->state_.pin_values[pin] = value;
    if (!changed) return;
    if (g_runtime->on_pin_changed)
        g_runtime->on_pin_changed(pin, value);
    else
        std::cout << ts() << "  pin " << pin
                  << " -> " << (value ? "HIGH" : "LOW") << "\n";
}

int ArduinoRuntime::impl_digitalRead(int pin) {
    if (!g_runtime || pin < 0 || pin >= 20) return 0;
    return g_runtime->state_.pin_values[pin];
}

void ArduinoRuntime::impl_analogWrite(int pin, int value) {
    std::cout << ts() << "  analogWrite(" << pin << ", " << value << ")\n";
}

int ArduinoRuntime::impl_analogRead(int pin) {
    if (!g_runtime || pin < 0 || pin >= 8) return 0;
    return g_runtime->state_.analog_values[pin];
}

void ArduinoRuntime::impl_delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

unsigned long ArduinoRuntime::impl_millis() {
    if (!g_runtime) return 0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() -
        g_runtime->state_.start_time).count();
    return (unsigned long)ms;
}

unsigned long ArduinoRuntime::impl_micros() {
    if (!g_runtime) return 0;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() -
        g_runtime->state_.start_time).count();
    return (unsigned long)us;
}

void ArduinoRuntime::impl_Serial_begin(int baud) {
    if (!g_runtime) return;
    g_runtime->state_.serial_started = true;
    g_runtime->state_.serial_baud    = baud;
    std::cout << ts() << "  Serial.begin(" << baud << ")\n";
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
    if (pin >= 0 && pin < 20)
        state_.pin_values[pin] = value;
}