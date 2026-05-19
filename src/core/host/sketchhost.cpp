#include "src/core/host/sketchhost.h"
#include <iostream>
#include <thread>
#include <chrono>

bool SketchHost::load(const std::string& dll_path) {
    dll_path_ = dll_path; // set dll path

    // Free any previously loaded DLL
    if (dll_.handle) {
        FreeLibrary(dll_.handle);
        dll_.handle   = nullptr;
        dll_.vb_init  = nullptr;
        dll_.vb_setup = nullptr;
        dll_.vb_loop  = nullptr;
    }

    // Copy to a temp file. Windows locks DLLs while loaded,
    // so the original must stay free for the compiler to overwrite.
    std::string tmp_path = dll_path + ".tmp.dll";
    CopyFileA(dll_path.c_str(), tmp_path.c_str(), FALSE); // Create the temp file

    HMODULE h = LoadLibraryA(tmp_path.c_str()); // load the file
    
    // if file is missing or errored
    if (!h) { 
        std::cerr << "[SketchHost] LoadLibrary failed: " << GetLastError() << "\n";
        return false;
    }

    // load arduino api processes
    dll_.handle   = h;
    dll_.vb_init  = (void(*)(ArduinoAPI*)) GetProcAddress(h, "vb_init");
    dll_.vb_setup = (void(*)())            GetProcAddress(h, "vb_setup");
    dll_.vb_loop  = (void(*)())            GetProcAddress(h, "vb_loop");

    // If the api failed to load the processes
    if (!dll_.vb_init || !dll_.vb_setup || !dll_.vb_loop) {
        std::cerr << "[SketchHost] Missing exports (vb_init / vb_setup / vb_loop)\n";
        FreeLibrary(h);
        dll_.handle = nullptr;
        return false;
    }
    // Save the DLL's last-modified timestamp.
    // needs_reload() compares against this to detect when the compiler
    // has written a new version of the DLL to disk.
    dll_.last_write_time = get_file_time(dll_path);

    // Build the API table from our runtime and inject it into the sketch
    api_ = runtime_.get_api();
    dll_.vb_init(&api_);
    dll_.vb_setup();
    return true;
}

void SketchHost::run_loop() {
    // executes one iteration of the sketch's loop()
    if (dll_.handle && dll_.vb_loop)
        dll_.vb_loop();
}

bool SketchHost::needs_reload() const {
    // Compare current file timestamp against saved timestamp.
    // Returns true if the DLL was modified on disk since last load.
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