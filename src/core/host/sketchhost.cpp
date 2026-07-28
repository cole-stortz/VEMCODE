#include "src/core/host/sketchhost.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>

static unsigned long current_pid() { return GetCurrentProcessId(); }

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
#include <unistd.h>

static unsigned long current_pid() { return (unsigned long)getpid(); }

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

// Each load() already removes its own immediate predecessor via
// last_tmp_path_, but a process that never destructs cleanly (crash,
// force-kill, an old GUI session left running) orphans its temp copy
// forever. This sweeps a sketch's directory on every successful load and
// keeps only the kMaxTmpFiles most recent, self-healing any pileup over
// time without needing per-process bookkeeping across separate processes.
static constexpr int kMaxTmpFiles = 8;

static void cleanup_old_tmp_files(const std::string& dll_path) {
    std::filesystem::path p(dll_path);
    std::filesystem::path dir = p.has_parent_path() ? p.parent_path() : std::filesystem::path(".");
    std::string prefix = p.filename().string() + ".";

    std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> candidates;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) return;
        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0) continue;
        if (name.size() < TMP_SUFFIX.size() ||
            name.compare(name.size() - TMP_SUFFIX.size(), TMP_SUFFIX.size(), TMP_SUFFIX) != 0)
            continue;
        std::error_code mtime_ec;
        auto mtime = std::filesystem::last_write_time(entry.path(), mtime_ec);
        if (mtime_ec) continue;
        candidates.emplace_back(mtime, entry.path());
    }
    if ((int)candidates.size() <= kMaxTmpFiles) return;

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; }); // newest first
    for (size_t i = kMaxTmpFiles; i < candidates.size(); ++i) {
        std::error_code rm_ec;
        std::filesystem::remove(candidates[i].second, rm_ec);
    }
}

SketchHost::~SketchHost() {
    runtime_.stop_threads(); // before unloading the DLL those threads call into
    if (dll_.handle) lib_close(dll_.handle);
    if (!last_tmp_path_.empty()) {
        std::error_code ec;
        std::filesystem::remove(last_tmp_path_, ec);
    }
}

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

    // Now that the old handle (if any) is closed, it's safe to remove its
    // temp file.
    if (!last_tmp_path_.empty()) {
        std::error_code ec;
        std::filesystem::remove(last_tmp_path_, ec);
        last_tmp_path_.clear();
    }

    // Copy to a temp file so the compiler can overwrite the original (the
    // original is locked while loaded on Windows). PID-scoped so two VEMCODE
    // processes running the same sketch never overwrite each other's mapped
    // file -- doing so while it's still dlopen'd elsewhere causes a SIGBUS.
    // Also counter-scoped so a *second* load() within the same process (Stop
    // then Run again, or an auto-recompile hot-reload) never reuses the same
    // path either: dlclose() doesn't guarantee the dynamic linker forgets
    // that (device, inode) pair, so overwriting and re-dlopen-ing the same
    // path afterward can crash dlsym() on the next load with a SIGSEGV deep
    // inside the dynamic linker's own symbol lookup -- reproduced in
    // isolation with a 30-line dlopen/dlclose/overwrite/dlopen repro, no
    // threads involved. A fresh name every load sidesteps the identity reuse
    // entirely instead of relying on dlclose() to fully undo it.
    std::string tmp_path = dll_path + "." + std::to_string(current_pid()) +
                            "." + std::to_string(load_count_++) + TMP_SUFFIX;
    lib_copy(dll_path, tmp_path);

    void* h = lib_open(tmp_path);
    if (!h) {
        std::cerr << "[SketchHost] load failed: " << lib_error() << "\n";
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return false;
    }
    last_tmp_path_ = tmp_path;
    cleanup_old_tmp_files(dll_path);

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

bool SketchHost::read_watched_variable(const std::string& name, WatchVarType type, std::string& out_value) const {
    if (!dll_.handle) return false;
    void* addr = lib_sym(dll_.handle, name.c_str());
    if (!addr) return false;

    std::ostringstream ss;
    switch (type) {
        case WatchVarType::Int:   ss << *reinterpret_cast<int*>(addr);           break;
        case WatchVarType::Float: ss << *reinterpret_cast<float*>(addr);         break;
        case WatchVarType::Long:  ss << *reinterpret_cast<long*>(addr);          break;
        case WatchVarType::ULong: ss << *reinterpret_cast<unsigned long*>(addr); break;
        case WatchVarType::Bool:  ss << (*reinterpret_cast<bool*>(addr) ? "true" : "false"); break;
    }
    out_value = ss.str();
    return true;
}
