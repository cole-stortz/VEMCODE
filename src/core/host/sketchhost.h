#pragma once
#include "src/core/runtime/arduinoruntime.h"
#include <string>
#include <filesystem>

struct SketchDLL {
    void* handle               = nullptr;
    void (*vb_init)(ArduinoAPI*) = nullptr;
    void (*vb_setup)()           = nullptr;
    void (*vb_loop)()            = nullptr;
    std::filesystem::file_time_type last_write_time = {};
};

// Type the Variable Watch panel reads a polled global as -- dlsym/GetProcAddress
// only gives back a void*, so the caller has to say how to interpret it.
enum class WatchVarType { Int, Float, Long, ULong, Bool };

class SketchHost {
public:
    ~SketchHost();

    bool load(const std::string& dll_path);  // safe to call again for hot-reload
    void run_loop();
    bool needs_reload() const;
    bool reload_if_changed();

    void inject_pin(int pin, int value);
    void inject_button_bounce(int pin, int value) { runtime_.inject_button_bounce(pin, value); }

    ArduinoRuntime& runtime() { return runtime_; }

    void inject_analog(int pin, int value) {
        runtime_.inject_analog(pin, value);
    }

    void inject_serial(const std::string& data) {
        runtime_.inject_serial(data);
    }

    void set_speed(float speed);

    void setProfile(BoardProfile p) { runtime_.setProfile(p); }

    void inject_pulse_duration(int pin, unsigned long micros) {
        runtime_.inject_pulse_duration(pin, micros);
    }

    void inject_color(int out_pin, int s2_pin, int s3_pin, int r, int g, int b) {
        runtime_.inject_color(out_pin, s2_pin, s3_pin, r, g, b);
    }

    void inject_keypad_wiring(const std::vector<int>& col_pins, const std::vector<int>& row_pins) {
        runtime_.inject_keypad_wiring(col_pins, row_pins);
    }

    void inject_keypad_press(int row_pin, int col_pin, bool pressed) {
        runtime_.inject_keypad_press(row_pin, col_pin, pressed);
    }

    void inject_dht(int pin, float temp_c, float humidity) {
        runtime_.inject_dht(pin, temp_c, humidity);
    }

    void inject_wire_device(int address, const std::vector<uint8_t>& bytes) {
        runtime_.inject_wire_device(address, bytes);
    }

    void inject_spi_bytes(const std::vector<uint8_t>& bytes) {
        runtime_.inject_spi_bytes(bytes);
    }

    // Resolves `name` as a global symbol in the loaded sketch DLL. Returns
    // false for a wrong name, a local variable, or no sketch loaded yet.
    bool read_watched_variable(const std::string& name, WatchVarType type, std::string& out_value) const;

    void reset_state() { runtime_.reset_state(); }

private:
    SketchDLL      dll_;
    ArduinoRuntime runtime_;
    ArduinoAPI     api_;
    std::string    dll_path_;

    static std::filesystem::file_time_type get_file_time(const std::string& path);
};
