#include "src/core/host/sketchhostthread.h"
#include <chrono>


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

    auto last_reload_check = std::chrono::steady_clock::now();

    while (running_) {
        host_.run_loop();

        auto now = std::chrono::steady_clock::now();
        if (now - last_reload_check >= std::chrono::milliseconds(500)) {
            last_reload_check = now;
            if (host_.reload_if_changed())
                emit sketchReloaded();
        }
    }
}
void SketchThread::injectPin(int pin, int value) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_pin(pin, value);
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