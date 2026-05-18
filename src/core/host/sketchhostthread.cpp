#include "src/core/host/sketchhostthread.h"

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
    wait();   // blocks until run() returns
}

// -------------------------------------------------------
// run() -- executes on the background thread
// This is the simulation loop.
// -------------------------------------------------------
void SketchThread::run() {
    // Wire up the runtime callbacks BEFORE loading the sketch.
    // We replace the std::cout calls with Qt signal emissions.
    // The UI connects to these signals to receive output.
    auto& runtime = host_.runtime();

    // Override Serial output -- emit signal instead of printing to console
    runtime.on_serial_output = [this](const std::string& text) {
        emit serialOutput(QString::fromStdString(text));
    };

    // Override pin change -- emit signal instead of printing to console
    runtime.on_pin_changed = [this](int pin, int value) {
        emit pinChanged(pin, value);
    };

    // Load the sketch DLL
    if (!host_.load(dll_path_.toStdString())) {
        emit loadFailed("Failed to load: " + dll_path_);
        return;
    }

    int loop_count = 0;

    while (running_) {
        host_.run_loop();
        loop_count++;

        // Check for hot-reload every 5 iterations
        if (loop_count % 5 == 0) {
            if (host_.reload_if_changed()) {
                emit sketchReloaded();
                loop_count = 0;
            }
        }
    }
}