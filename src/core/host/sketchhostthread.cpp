#include "src/core/host/sketchhostthread.h"
#include <chrono>
#include <csignal>
#include <setjmp.h>

// Windows has no sigaction/sigsetjmp/siglongjmp -- plain signal()/setjmp()/
// longjmp() cover the same SIGFPE/SIGSEGV recovery there (and the CRT
// already resets the handler to SIG_DFL before invoking it, same as
// SA_RESETHAND below).
#ifdef _WIN32
static thread_local jmp_buf     tl_crash_jmp;
#else
static thread_local sigjmp_buf  tl_crash_jmp;
#endif
static thread_local bool        tl_in_sketch = false;
static thread_local int         tl_crash_sig  = 0;
static thread_local bool        tl_exec_locked = false; // is exec_mutex() held right now?

static void sketch_signal_handler(int sig) {
    if (!tl_in_sketch) { ::signal(sig, SIG_DFL); ::raise(sig); return; }
    tl_crash_sig = sig;
#ifdef _WIN32
    ::longjmp(tl_crash_jmp, 1);
#else
    ::siglongjmp(tl_crash_jmp, 1);
#endif
}

static WatchVarType parse_watch_type(const QString& type) {
    QString t = type.trimmed().toLower();
    if (t == "float")           return WatchVarType::Float;
    if (t == "long")            return WatchVarType::Long;
    if (t == "ulong" || t == "unsigned long") return WatchVarType::ULong;
    if (t == "bool")            return WatchVarType::Bool;
    return WatchVarType::Int;
}


SketchThread::SketchThread(QObject* parent)
    : QThread(parent)
{
}

void SketchThread::startSketch(const QString& dll_path) {
    // If already running, stop first
    stopSketch();

    dll_path_ = dll_path;
    running_  = true;
    start();  // launches run() on the background thread
}

void SketchThread::stopSketch() {
    running_ = false;
    host_.runtime().request_stop();
    wait();
    host_.runtime().clear_stop();
}

void SketchThread::run() {
    // Wire up the runtime callbacks BEFORE loading the sketch.
    // We replace the std::cout calls with Qt signal emissions.
    // The UI connects to these signals to receive output.
    auto& runtime = host_.runtime();

    // Override Serial output -- emit signal instead of printing to console
    runtime.on_serial_output = [this](const std::string& text) {
        emit serialOutput(QString::fromStdString(text));
    };

    runtime.on_serial1_output = [this](const std::string& text) {
        emit serial1Output(QString::fromStdString(text));
    };

    runtime.on_serial2_output = [this](const std::string& text) {
        emit serial2Output(QString::fromStdString(text));
    };

    runtime.on_soft_serial_output = [this](int rxPin, const std::string& text) {
        emit softSerialOutput(rxPin, QString::fromStdString(text));
    };

    // Override pin change -- emit signal instead of printing to console
    runtime.on_pin_changed = [this](int pin, int value) {
        emit pinChanged(pin, value);
    };

    // Check to see if a variable changed and emit that signal
    runtime.on_variable_changed = [this](const std::string& name, int value) {
        emit variableChanged(QString::fromStdString(name), QString::number(value));
    };

    // LCD text output
    runtime.on_lcd_print = [this](int pin, int row, const std::string& text) {
        emit lcdPrint(pin, row, QString::fromStdString(text));
    };

    // LED Matrix (MAX7219) row output
    runtime.on_matrix_row = [this](int pin, int addr, int row, int bits) {
        emit matrixRowChanged(pin, addr, row, bits);
    };

    // Watchdog
    runtime.on_watchdog_reset = [this](){
        emit watchdogReset();
    };

    // Sleep
    runtime.on_sleep_changed = [this](bool sleeping) {
        emit sleepChanged(sleeping);
    };

    // Load the sketch DLL. setup() runs inside load(), on this same thread,
    // before the while(running_) loop below ever locks exec_mutex() -- but
    // setup() can itself call delay()/delayMicroseconds(), which
    // unconditionally unlock/relock exec_mtx_ to let ISRs preempt them.
    // Without holding the lock here first, that unlock() targets a mutex
    // this thread doesn't actually own (undefined behavior, observed to
    // hang later lock attempts).
    bool loaded;
    {
        std::lock_guard<std::mutex> exec_lock(runtime.exec_mutex());
        loaded = host_.load(dll_path_.toStdString());
    }
    if (!loaded) {
        emit loadFailed("Failed to load: " + dll_path_);
        return;
    }

    // Install signal handlers so SIGFPE/SIGSEGV from sketch code are recoverable
#ifdef _WIN32
    auto old_fpe  = ::signal(SIGFPE,  sketch_signal_handler);
    auto old_segv = ::signal(SIGSEGV, sketch_signal_handler);
#else
    struct sigaction sa = {}, old_fpe, old_segv;
    sa.sa_handler = sketch_signal_handler;
    sa.sa_flags   = SA_RESETHAND; // one-shot: reverts to SIG_DFL after first delivery
    sigaction(SIGFPE,  &sa, &old_fpe);
    sigaction(SIGSEGV, &sa, &old_segv);
#endif
    tl_in_sketch = true;

#ifdef _WIN32
    if (::setjmp(tl_crash_jmp) != 0) {
#else
    if (::sigsetjmp(tl_crash_jmp, 1) != 0) {
#endif
        QString reason = (tl_crash_sig == SIGFPE)
            ? "Sketch crashed — check for division by zero or invalid arithmetic"
            : "Sketch crashed — check for null pointer or out-of-bounds array access";
        emit sketchCrashed(reason);
        tl_in_sketch = false;
        if (tl_exec_locked) { host_.runtime().exec_mutex().unlock(); tl_exec_locked = false; }
#ifdef _WIN32
        ::signal(SIGFPE,  old_fpe);
        ::signal(SIGSEGV, old_segv);
#else
        sigaction(SIGFPE,  &old_fpe,  nullptr);
        sigaction(SIGSEGV, &old_segv, nullptr);
#endif
        return;
    }

    auto last_reload_check = std::chrono::steady_clock::now();
    auto last_watch_poll   = std::chrono::steady_clock::now();

    while (running_) {
        try {
            host_.runtime().exec_mutex().lock();
            tl_exec_locked = true;
            host_.run_loop();
            tl_exec_locked = false;
            host_.runtime().exec_mutex().unlock();
        } catch (const std::exception& e) {
            tl_exec_locked = false;
            host_.runtime().exec_mutex().unlock();
            emit sketchCrashed(QString("Sketch crashed: %1").arg(e.what()));
            break;
        } catch (...) {
            tl_exec_locked = false;
            host_.runtime().exec_mutex().unlock();
            emit sketchCrashed("Sketch crashed — unknown error");
            break;
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_reload_check >= std::chrono::milliseconds(500)) {
            last_reload_check = now;
            if (host_.reload_if_changed())
                emit sketchReloaded();
        }

        // Polled here (same thread as vb_loop()) so reads of the sketch's own
        // globals never race its writes.
        if (now - last_watch_poll >= std::chrono::milliseconds(100)) {
            last_watch_poll = now;
            std::vector<std::pair<QString, QString>> watches;
            { QMutexLocker lock(&watch_mutex_); watches = watchList_; }
            for (const auto& [name, typeStr] : watches) {
                std::string value;
                if (host_.read_watched_variable(name.toStdString(), parse_watch_type(typeStr), value))
                    emit variableChanged(name, QString::fromStdString(value));
                else
                    emit variableChanged(name, "?");
            }
        }
    }

    tl_in_sketch = false;
#ifdef _WIN32
    ::signal(SIGFPE,  old_fpe);
    ::signal(SIGSEGV, old_segv);
#else
    sigaction(SIGFPE,  &old_fpe,  nullptr);
    sigaction(SIGSEGV, &old_segv, nullptr);
#endif
}
void SketchThread::injectPin(int pin, int value) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_pin(pin, value);
}

void SketchThread::injectButtonBounce(int pin, int value) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_button_bounce(pin, value);
}

void SketchThread::setAnalogNoise(bool enabled) {
    host_.runtime().set_analog_noise(enabled);
}

void SketchThread::injectAnalog(int pin, int value) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_analog(pin, value);
}

void SketchThread::setSpeed(float speed){
    host_.set_speed(speed);
}

void SketchThread::injectSerial(const QString& data) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_serial(data.toStdString());
}

void SketchThread::injectPulseDuration(int pin, unsigned long micros) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_pulse_duration(pin, micros);
}

void SketchThread::injectColor(int out_pin, int s2_pin, int s3_pin, int r, int g, int b) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_color(out_pin, s2_pin, s3_pin, r, g, b);
}

void SketchThread::injectKeypadWiring(const std::vector<int>& col_pins, const std::vector<int>& row_pins) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_keypad_wiring(col_pins, row_pins);
}

void SketchThread::injectKeypadPress(int row_pin, int col_pin, bool pressed) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_keypad_press(row_pin, col_pin, pressed);
}

void SketchThread::injectDht(int pin, float temp_c, float humidity) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_dht(pin, temp_c, humidity);
}

void SketchThread::injectWireDevice(int address, const std::vector<uint8_t>& bytes) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_wire_device(address, bytes);
}

void SketchThread::injectSpiBytes(const std::vector<uint8_t>& bytes) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_spi_bytes(bytes);
}

void SketchThread::setProfile(BoardProfile p) {
    QMutexLocker lock(&inject_mutex_);
    host_.setProfile(p);
}

void SketchThread::injectSoftSerial(int rxPin, const QString& data) {
    QMutexLocker lock(&inject_mutex_);
    host_.runtime().inject_soft_serial(rxPin, data.toStdString());
}

void SketchThread::resetRuntimeState() {
    QMutexLocker lock(&inject_mutex_);
    host_.reset_state();
}

void SketchThread::setWatchList(std::vector<std::pair<QString, QString>> vars) {
    QMutexLocker lock(&watch_mutex_);
    watchList_ = std::move(vars);
}