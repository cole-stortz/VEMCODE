#include "src/core/host/sketchhost.h"
#include <iostream>
#include <thread>
#include <chrono>

// -------------------------------------------------------
// load() -- loads the DLL, extracts function pointers,
//           injects the API, calls setup().
// -------------------------------------------------------
bool SketchHost::load(const std::string& dll_path) {
    dll_path_ = dll_path;

    // Free any previously loaded DLL
    if (dll_.handle) {
        FreeLibrary(dll_.handle);
        dll_.handle   = nullptr;
        dll_.vb_init  = nullptr;
        dll_.vb_setup = nullptr;
        dll_.vb_loop  = nullptr;
    }

    // Copy to a temp file -- Windows locks DLLs while loaded,
    // so the original must stay free for the compiler to overwrite.
    std::string tmp_path = dll_path + ".tmp.dll";
    CopyFileA(dll_path.c_str(), tmp_path.c_str(), FALSE);

    HMODULE h = LoadLibraryA(tmp_path.c_str());
    if (!h) {
        std::cerr << "[SketchHost] LoadLibrary failed: " << GetLastError() << "\n";
        return false;
    }

    dll_.handle   = h;
    dll_.vb_init  = (void(*)(ArduinoAPI*)) GetProcAddress(h, "vb_init");
    dll_.vb_setup = (void(*)())            GetProcAddress(h, "vb_setup");
    dll_.vb_loop  = (void(*)())            GetProcAddress(h, "vb_loop");

    if (!dll_.vb_init || !dll_.vb_setup || !dll_.vb_loop) {
        std::cerr << "[SketchHost] Missing exports (vb_init / vb_setup / vb_loop)\n";
        FreeLibrary(h);
        dll_.handle = nullptr;
        return false;
    }

    dll_.last_write_time = get_file_time(dll_path);

    // Build the API table from our runtime and inject it into the sketch
    api_ = runtime_.get_api();
    dll_.vb_init(&api_);
    dll_.vb_setup();
    return true;
}

// -------------------------------------------------------
// run_loop() -- executes one iteration of the sketch's loop()
// -------------------------------------------------------
void SketchHost::run_loop() {
    if (dll_.handle && dll_.vb_loop)
        dll_.vb_loop();
}

// -------------------------------------------------------
// Hot-reload helpers
// -------------------------------------------------------
bool SketchHost::needs_reload() const {
    if (dll_path_.empty()) return false;
    FILETIME current = get_file_time(dll_path_);
    return file_time_changed(current, dll_.last_write_time);
}

bool SketchHost::reload_if_changed() {
    if (!needs_reload()) return false;
    // Small sleep to let the compiler finish writing the file
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "\n[SketchHost] DLL changed -- hot reloading...\n\n";
    return load(dll_path_);
}

// -------------------------------------------------------
// File time helpers (Windows-specific)
// -------------------------------------------------------
FILETIME SketchHost::get_file_time(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &info))
        return info.ftLastWriteTime;
    return {};
}

bool SketchHost::file_time_changed(FILETIME a, FILETIME b) {
    return CompareFileTime(&a, &b) != 0;
}

void SketchHost::inject_pin(int pin, int value) {
    runtime_.inject_pin(pin, value);
}