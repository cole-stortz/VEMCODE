#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QFile>
#include "src/core/host/sketchhostthread.h"
#include "src/core/build/compiler.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/circuitdetector.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onSerialOutput(QString text);
    void onPinChanged(int pin, int value);
    void onSketchReloaded();
    void onLoadFailed(QString reason);
    void onRunClicked();
    void onStopClicked();
    void onOpenClicked();

private:
    void setupToolbar(QWidget* parent, QVBoxLayout* layout);
    void setupMainArea(QWidget* parent, QVBoxLayout* layout);
    QWidget* buildEditorPanel();
    QWidget* buildCanvasPanel();
    QWidget* buildDebugPanel();

    // Toolbar
    QPushButton*    runButton_      = nullptr;
    QPushButton*    stopButton_     = nullptr;
    QLabel*         boardLabel_     = nullptr;

    // Editor panel (left)
    QPlainTextEdit* codeEditor_     = nullptr;

    // Canvas panel (top right)
    CanvasWidget*   canvasWidget_   = nullptr;

    // Debug panel (bottom right)
    QTabWidget*     debugTabs_      = nullptr;
    QPlainTextEdit* serialMonitor_  = nullptr;

    // Simulation
    SketchThread*      sketchThread_  = nullptr;
    QString            currentSketchPath_;

    // Circuit detection
    CircuitDetector    detector_;
};