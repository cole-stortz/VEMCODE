#include "src/core/host/sketchhostthread.h"
#include <chrono>
#include <csignal>
#include <setjmp.h>

static thread_local sigjmp_buf  tl_crash_jmp;
static thread_local bool        tl_in_sketch = false;
static thread_local int         tl_crash_sig  = 0;

static void sketch_signal_handler(int sig) {
    if (!tl_in_sketch) { ::signal(sig, SIG_DFL); ::raise(sig); return; }
    tl_crash_sig = sig;
    ::siglongjmp(tl_crash_jmp, 1);
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
        emit variableChanged(QString::fromStdString(name), value);
    };

    // LCD text output
    runtime.on_lcd_print = [this](int pin, int row, const std::string& text) {
        emit lcdPrint(pin, row, QString::fromStdString(text));
    };  

    // Load the sketch DLL
    if (!host_.load(dll_path_.toStdString())) {
        emit loadFailed("Failed to load: " + dll_path_);
        return;
    }

    // Install signal handlers so SIGFPE/SIGSEGV from sketch code are recoverable
    struct sigaction sa = {}, old_fpe, old_segv;
    sa.sa_handler = sketch_signal_handler;
    sa.sa_flags   = SA_RESETHAND; // one-shot: reverts to SIG_DFL after first delivery
    sigaction(SIGFPE,  &sa, &old_fpe);
    sigaction(SIGSEGV, &sa, &old_segv);
    tl_in_sketch = true;

    if (::sigsetjmp(tl_crash_jmp, 1) != 0) {
        QString reason = (tl_crash_sig == SIGFPE)
            ? "Sketch crashed — check for division by zero or invalid arithmetic"
            : "Sketch crashed — check for null pointer or out-of-bounds array access";
        emit sketchCrashed(reason);
        tl_in_sketch = false;
        sigaction(SIGFPE,  &old_fpe,  nullptr);
        sigaction(SIGSEGV, &old_segv, nullptr);
        return;
    }

    auto last_reload_check = std::chrono::steady_clock::now();

    while (running_) {
        try {
            host_.run_loop();
        } catch (const std::exception& e) {
            emit sketchCrashed(QString("Sketch crashed: %1").arg(e.what()));
            break;
        } catch (...) {
            emit sketchCrashed("Sketch crashed — unknown error");
            break;
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_reload_check >= std::chrono::milliseconds(500)) {
            last_reload_check = now;
            if (host_.reload_if_changed())
                emit sketchReloaded();
        }
    }

    tl_in_sketch = false;
    sigaction(SIGFPE,  &old_fpe,  nullptr);
    sigaction(SIGSEGV, &old_segv, nullptr);
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

void SketchThread::setProfile(BoardProfile p) {
    QMutexLocker lock(&inject_mutex_);
    host_.setProfile(p);
}

void SketchThread::injectSoftSerial(int rxPin, const QString& data) {
    QMutexLocker lock(&inject_mutex_);
    host_.runtime().inject_soft_serial(rxPin, data.toStdString());
}