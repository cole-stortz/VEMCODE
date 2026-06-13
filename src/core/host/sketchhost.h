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

private:
    SketchDLL      dll_;
    ArduinoRuntime runtime_;
    ArduinoAPI     api_;
    std::string    dll_path_;

    static std::filesystem::file_time_type get_file_time(const std::string& path);
};
