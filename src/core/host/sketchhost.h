#pragma once
#include "src/core/runtime/arduinoruntime.h"
#include <string>
#include <filesystem>

// Holds the loaded sketch shared library and the three function pointers extracted from it.
struct SketchDLL {
    void* handle               = nullptr;
    void (*vb_init)(ArduinoAPI*) = nullptr;
    void (*vb_setup)()           = nullptr;
    void (*vb_loop)()            = nullptr;
    std::filesystem::file_time_type last_write_time = {};
};

// SketchHost manages the full lifecycle of a sketch shared library:
//   - Loading it from disk
//   - Injecting the ArduinoAPI table
//   - Running setup() and loop()
//   - Hot-reloading when the library file changes on disk
//
// In the Qt app this is owned by SketchThread and run on a worker thread.
class SketchHost {
public:
    // Load a sketch shared library from disk, inject the API, and call setup().
    // Returns true on success. Safe to call again for hot-reload.
    bool load(const std::string& dll_path);

    // Call the sketch's loop() function once.
    void run_loop();

    // Check if the library file on disk has changed since last load.
    bool needs_reload() const;

    // Reload if the file changed. Returns true if a reload happened.
    bool reload_if_changed();

    void inject_pin(int pin, int value);

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

private:
    SketchDLL      dll_;
    ArduinoRuntime runtime_;
    ArduinoAPI     api_;
    std::string    dll_path_;

    static std::filesystem::file_time_type get_file_time(const std::string& path);
};
