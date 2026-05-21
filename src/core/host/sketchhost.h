#pragma once
#include "src/core/runtime/arduinoruntime.h"
#include <string>
#include <windows.h>

// Holds the loaded sketch DLL and the three function pointers extracted from it.
struct SketchDLL {
    HMODULE handle          = nullptr;
    void (*vb_init)(ArduinoAPI*) = nullptr;
    void (*vb_setup)()           = nullptr;
    void (*vb_loop)()            = nullptr;
    FILETIME last_write_time     = {};
};

// SketchHost manages the full lifecycle of a sketch DLL:
//   - Loading it from disk
//   - Injecting the ArduinoAPI table
//   - Running setup() and loop()
//   - Hot-reloading when the DLL file changes on disk
//
// In the Qt app this will be owned by SketchThread and run on a worker thread.
// Right now it has no Qt dependency so it can be tested standalone.
class SketchHost {
public:
    // Load a sketch DLL from disk, inject the API, and call setup().
    // Returns true on success. Safe to call again for hot-reload.
    bool load(const std::string& dll_path);

    // Call the sketch's loop() function once.
    void run_loop();

    // Check if the DLL file on disk has changed since last load.
    // Returns true if a reload is needed.
    bool needs_reload() const;

    // Reload if the file changed. Returns true if a reload happened.
    bool reload_if_changed();

    void inject_pin(int pin, int value);

    // Direct access to the runtime for reading pin state, serial output, etc.
    ArduinoRuntime& runtime() { return runtime_; }

    void inject_analog(int pin, int value) {
        runtime_.inject_analog(pin, value);
    }

    void inject_serial(const std::string& data) {
        runtime_.inject_serial(data);
    }

    void set_speed(float speed);

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

    static FILETIME get_file_time(const std::string& path);
    static bool     file_time_changed(FILETIME a, FILETIME b);
};