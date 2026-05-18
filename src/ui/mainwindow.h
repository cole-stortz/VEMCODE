#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include "src/core/host/sketchhostthread.h"

// MainWindow is the top-level Qt window.
// For v0.1 it contains:
//   - A serial monitor (QPlainTextEdit)
//   - A pin state label (shows pin 13 HIGH/LOW)
//   - Run / Stop buttons
//
// It owns a SketchThread and connects to its signals to receive
// serial output and pin changes from the running sketch.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // Connected to SketchThread signals
    void onSerialOutput(QString text);
    void onPinChanged(int pin, int value);
    void onSketchReloaded();
    void onLoadFailed(QString reason);

    // Button handlers
    void onRunClicked();
    void onStopClicked();

private:
    void setupUi();

    // Widgets
    QPlainTextEdit* serialMonitor_  = nullptr;
    QLabel*         pinStateLabel_  = nullptr;
    QPushButton*    runButton_      = nullptr;
    QPushButton*    stopButton_     = nullptr;

    // Simulation thread
    SketchThread*   sketchThread_   = nullptr;
};