// -------------------------------------------------------
// VirtualBench Host -- proof of concept
//
// Demonstrates:
//   1. Implementing the ArduinoAPI (fake digitalWrite, delay, etc.)
//   2. Loading sketch.dll at runtime via LoadLibrary
//   3. Injecting the API into the DLL
//   4. Running the sketch loop
//   5. Hot-reloading: watching for sketch.dll changes and reloading live
//
// Build sketch DLL:
//   g++ -shared -o sketch.dll sketch.cpp -std=c++17
//
// Build host:
//   g++ -o host.exe host.cpp -std=c++17
//
// Run:
//   host.exe
// -------------------------------------------------------

#include "virtual_arduino.h"

#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>

// -------------------------------------------------------
// Runtime state -- what the host tracks on behalf of the sketch
// -------------------------------------------------------
struct RuntimeState {
    int  pin_modes[20]  = {};   // INPUT/OUTPUT per pin
    int  pin_values[20] = {};   // HIGH/LOW per pin
    int  analog_values[8] = {}; // Simulated analog pin values (A0-A7)
    bool serial_started = false;
    int  serial_baud    = 0;
    std::chrono::steady_clock::time_point start_time;
};

static RuntimeState state;

// Timestamp helper for console output
std::string ts() {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - state.start_time).count();
    std::ostringstream ss;
    ss << "[" << std::setw(8) << us / 1000 << "ms] ";
    return ss.str();
}

// -------------------------------------------------------
// ArduinoAPI implementations
// These are what actually run when the sketch calls digitalWrite() etc.
// In VirtualBench proper, these will emit Qt signals to update the UI.
// -------------------------------------------------------

void impl_pinMode(int pin, int mode) {
    if (pin < 0 || pin >= 20) return;
    state.pin_modes[pin] = mode;
    const char* mode_str = (mode == 1) ? "OUTPUT" : 
                           (mode == 2) ? "INPUT_PULLUP" : "INPUT";
    std::cout << ts() << "  pinMode(" << pin << ", " << mode_str << ")\n";
}

void impl_digitalWrite(int pin, int value) {
    if (pin < 0 || pin >= 20) return;
    bool changed = (state.pin_values[pin] != value);
    state.pin_values[pin] = value;
    // Only print when the value actually changes -- less noise
    if (changed) {
        std::cout << ts() << "  pin " << pin 
                  << " -> " << (value ? "HIGH ▲" : "LOW  ▼") << "\n";
    }
}

int impl_digitalRead(int pin) {
    if (pin < 0 || pin >= 20) return 0;
    return state.pin_values[pin];
}

void impl_analogWrite(int pin, int value) {
    std::cout << ts() << "  analogWrite(" << pin << ", " << value << ")\n";
}

int impl_analogRead(int pin) {
    if (pin < 0 || pin >= 8) return 0;
    return state.analog_values[pin]; // Host can set these to simulate sensors
}

void impl_delay(unsigned long ms) {
    // Real delay -- in VirtualBench proper this will be virtual (
    // advancing a simulation clock instead of actually sleeping).
    // For the POC, we sleep at 10x speed so you can see output quickly.
    std::this_thread::sleep_for(std::chrono::milliseconds(ms / 10));
}

unsigned long impl_millis() {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - state.start_time).count();
    return (unsigned long)ms;
}

unsigned long impl_micros() {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - state.start_time).count();
    return (unsigned long)us;
}

void impl_Serial_begin(int baud) {
    state.serial_started = true;
    state.serial_baud = baud;
    std::cout << ts() << "  Serial.begin(" << baud << ")\n";
}

void impl_Serial_print(const char* s) {
    std::cout << ts() << "  Serial >> " << s;
}

void impl_Serial_println(const char* s) {
    std::cout << ts() << "  Serial >> " << s << "\n";
}

// -------------------------------------------------------
// Build the API table -- one place where all function pointers live
// -------------------------------------------------------
ArduinoAPI build_api() {
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

// -------------------------------------------------------
// Sketch DLL loader
// -------------------------------------------------------
struct SketchDLL {
    HMODULE handle  = nullptr;
    void (*vb_init) (ArduinoAPI*) = nullptr;
    void (*vb_setup)()            = nullptr;
    void (*vb_loop) ()            = nullptr;
    FILETIME last_write_time      = {};
};

FILETIME get_file_time(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &info))
        return info.ftLastWriteTime;
    return {};
}

bool file_time_changed(FILETIME a, FILETIME b) {
    return CompareFileTime(&a, &b) != 0;
}

bool load_sketch(SketchDLL& dll, const char* path, ArduinoAPI& api) {
    // If already loaded, free it first
    if (dll.handle) {
        FreeLibrary(dll.handle);
        dll.handle   = nullptr;
        dll.vb_init  = nullptr;
        dll.vb_setup = nullptr;
        dll.vb_loop  = nullptr;
    }

    // Windows locks DLLs while loaded -- copy to a temp file to allow
    // the compiler to overwrite sketch.dll while we're running
    std::string tmp_path = std::string(path) + ".tmp.dll";
    CopyFileA(path, tmp_path.c_str(), FALSE);

    HMODULE h = LoadLibraryA(tmp_path.c_str());
    if (!h) {
        std::cerr << "  LoadLibrary failed: " << GetLastError() << "\n";
        return false;
    }

    dll.handle   = h;
    dll.vb_init  = (void(*)(ArduinoAPI*)) GetProcAddress(h, "vb_init");
    dll.vb_setup = (void(*)())            GetProcAddress(h, "vb_setup");
    dll.vb_loop  = (void(*)())            GetProcAddress(h, "vb_loop");

    if (!dll.vb_init || !dll.vb_setup || !dll.vb_loop) {
        std::cerr << "  Missing exports (vb_init/vb_setup/vb_loop)\n";
        FreeLibrary(h);
        dll.handle = nullptr;
        return false;
    }

    dll.last_write_time = get_file_time(path);

    // Inject the API and run setup()
    dll.vb_init(&api);
    dll.vb_setup();
    return true;
}

// -------------------------------------------------------
// Main loop
// -------------------------------------------------------
int main() {
    const char* dll_path = "sketch.dll";
    state.start_time = std::chrono::steady_clock::now();

    std::cout << "=== VirtualBench Host (POC) ===\n";
    std::cout << "Watching: " << dll_path << "\n";
    std::cout << "Edit sketch.cpp, recompile, and watch it hot-reload!\n\n";

    ArduinoAPI api = build_api();
    SketchDLL  dll;

    if (!load_sketch(dll, dll_path, api)) {
        std::cerr << "Failed to load sketch.dll. Build it first:\n";
        std::cerr << "  g++ -shared -o sketch.dll sketch.cpp -std=c++17\n";
        return 1;
    }

    int loop_count = 0;

    while (true) {
        // Run one loop() iteration
        if (dll.handle && dll.vb_loop) {
            dll.vb_loop();
            loop_count++;
        }

        // Every 5 loop iterations, check if the DLL file changed
        if (loop_count % 5 == 0) {
            FILETIME current = get_file_time(dll_path);
            if (file_time_changed(current, dll.last_write_time)) {
                // Small sleep to let the compiler finish writing
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::cout << "\n>>> sketch.dll changed -- hot reloading...\n\n";
                load_sketch(dll, dll_path, api);
                loop_count = 0;
            }
        }
    }

    return 0;
}