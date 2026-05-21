#pragma once
#include "src/core/host/sketchhost.h"
#include <QThread>
#include <QString>
#include <atomic>
#include <QMutex>

// SketchThread runs SketchHost on a background thread so the simulation
// never blocks the Qt UI thread.
//
// Usage:
//   SketchThread* thread = new SketchThread(this);
//   connect(thread, &SketchThread::serialOutput, monitor, &SerialMonitor::append);
//   connect(thread, &SketchThread::pinChanged, canvas, &Canvas::updatePin);
//   thread->startSketch("path/to/sketch.dll");
//
// The thread emits signals when the sketch produces output.
// The UI connects to those signals to update itself.
class SketchThread : public QThread {
    Q_OBJECT

public:
    explicit SketchThread(QObject* parent = nullptr);

    // Load and start running a sketch DLL.
    // Safe to call again to hot-reload a new DLL.
    void startSketch(const QString& dll_path);

    // Stop the simulation loop and wait for the thread to finish.
    void stopSketch();

    void injectPin(int pin, int value); 

    void injectAnalog(int pin, int value);

    void setSpeed(float speed);

    void injectSerial(const QString& data);

    void injectPulseDuration(int pin, unsigned long micros);

signals:
    // Emitted when the sketch calls Serial.print() or Serial.println()
    void serialOutput(QString text);

    // Emitted when the sketch calls digitalWrite() and the value changes
    void pinChanged(int pin, int value);

    // Emitted when a hot-reload happens
    void sketchReloaded();

    // Emitted if the DLL fails to load
    void loadFailed(QString reason);

    // Check to see if a variable changed for watch vairable
    void variableChanged(QString name, int value);

protected:
    // QThread entry point -- runs on the background thread
    void run() override;

private:
    SketchHost    host_;
    QString       dll_path_;
    std::atomic<bool> running_{false};
    QMutex        inject_mutex_;    
};