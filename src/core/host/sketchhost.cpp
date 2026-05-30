#include "src/core/host/sketchhost.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>

static void* lib_open(const std::string& path) {
    return LoadLibraryA(path.c_str());
}
static void lib_close(void* h) {
    FreeLibrary((HMODULE)h);
}
static void* lib_sym(void* h, const char* name) {
    return (void*)GetProcAddress((HMODULE)h, name);
}
static std::string lib_error() {
    return std::to_string(GetLastError());
}
static void lib_copy(const std::string& src, const std::string& dst) {
    CopyFileA(src.c_str(), dst.c_str(), FALSE);
}

#else
#include <dlfcn.h>

static void* lib_open(const std::string& path) {
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
}
static void lib_close(void* h) {
    dlclose(h);
}
static void* lib_sym(void* h, const char* name) {
    return dlsym(h, name);
}
static std::string lib_error() {
    const char* e = dlerror();
    return e ? e : "unknown error";
}
static void lib_copy(const std::string& src, const std::string& dst) {
    std::error_code ec;
    std::filesystem::copy_file(src, dst,
        std::filesystem::copy_options::overwrite_existing, ec);
}
#endif

// Platform-appropriate temp suffix (keeps the original free for the compiler to overwrite)
#ifdef _WIN32
static const std::string TMP_SUFFIX = ".tmp.dll";
#elif defined(__APPLE__)
static const std::string TMP_SUFFIX = ".tmp.dylib";
#else
static const std::string TMP_SUFFIX = ".tmp.so";
#endif

bool SketchHost::load(const std::string& dll_path) {
    dll_path_ = dll_path;

    // Free any previously loaded library
    if (dll_.handle) {
        lib_close(dll_.handle);
        dll_.handle   = nullptr;
        dll_.vb_init  = nullptr;
        dll_.vb_setup = nullptr;
        dll_.vb_loop  = nullptr;
    }

    // Copy to a temp file so the compiler can overwrite the original.
    // On Windows the original is locked while loaded; on Linux the copy
    // is harmless and keeps behaviour consistent across platforms.
    std::string tmp_path = dll_path + TMP_SUFFIX;
    lib_copy(dll_path, tmp_path);

    void* h = lib_open(tmp_path);
    if (!h) {
        std::cerr << "[SketchHost] load failed: " << lib_error() << "\n";
        return false;
    }

    dll_.handle   = h;
    dll_.vb_init  = (void(*)(ArduinoAPI*)) lib_sym(h, "vb_init");
    dll_.vb_setup = (void(*)())            lib_sym(h, "vb_setup");
    dll_.vb_loop  = (void(*)())            lib_sym(h, "vb_loop");

    if (!dll_.vb_init || !dll_.vb_setup || !dll_.vb_loop) {
        std::cerr << "[SketchHost] Missing exports (vb_init / vb_setup / vb_loop)\n";
        lib_close(h);
        dll_.handle = nullptr;
        return false;
    }

    dll_.last_write_time = get_file_time(dll_path);

    api_ = runtime_.get_api();
    dll_.vb_init(&api_);
    dll_.vb_setup();
    return true;
}

void SketchHost::run_loop() {
    if (dll_.handle && dll_.vb_loop)
        dll_.vb_loop();
}

bool SketchHost::needs_reload() const {
    if (dll_path_.empty()) return false;
    return get_file_time(dll_path_) != dll_.last_write_time;
}

bool SketchHost::reload_if_changed() {
    if (!needs_reload()) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "\n[SketchHost] Library changed -- hot reloading...\n\n";
    return load(dll_path_);
}

std::filesystem::file_time_type SketchHost::get_file_time(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : t;
}

void SketchHost::inject_pin(int pin, int value) {
    runtime_.inject_pin(pin, value);
}

void SketchHost::set_speed(float speed) {
    runtime_.set_speed_multiplier(speed);
}
