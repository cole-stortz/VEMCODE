#include "src/ui/mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <QStatusBar>
#include <QFont>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("VirtualBench");
    resize(900, 600);
    setupUi();

    // Create the sketch thread
    sketchThread_ = new SketchThread(this);

    // Wire up signals from the thread to our slots.
    // Qt automatically queues these across threads -- safe to update UI here.
    connect(sketchThread_, &SketchThread::serialOutput,
            this, &MainWindow::onSerialOutput);
    connect(sketchThread_, &SketchThread::pinChanged,
            this, &MainWindow::onPinChanged);
    connect(sketchThread_, &SketchThread::sketchReloaded,
            this, &MainWindow::onSketchReloaded);
    connect(sketchThread_, &SketchThread::loadFailed,
            this, &MainWindow::onLoadFailed);
}

MainWindow::~MainWindow() {
    if (sketchThread_) {
        sketchThread_->stopSketch();
    }
}

// -------------------------------------------------------
// setupUi -- builds the window layout in code (no .ui file)
// -------------------------------------------------------
void MainWindow::setupUi() {
    // Central widget with vertical layout
    QWidget*     central = new QWidget(this);
    QVBoxLayout* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    setCentralWidget(central);

    // Pin state label
    pinStateLabel_ = new QLabel("Pin 13: ---", this);
    pinStateLabel_->setStyleSheet("font-size: 14px; font-weight: bold; padding: 4px;");
    layout->addWidget(pinStateLabel_);

    // Serial monitor
    serialMonitor_ = new QPlainTextEdit(this);
    serialMonitor_->setReadOnly(true);
    serialMonitor_->setMaximumBlockCount(1000); // keep last 1000 lines
    QFont mono("Courier New", 10);
    mono.setStyleHint(QFont::Monospace);
    serialMonitor_->setFont(mono);
    serialMonitor_->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    layout->addWidget(serialMonitor_);

    // Button row
    QHBoxLayout* buttonRow = new QHBoxLayout();
    runButton_  = new QPushButton("Run", this);
    stopButton_ = new QPushButton("Stop", this);
    stopButton_->setEnabled(false);
    buttonRow->addWidget(runButton_);
    buttonRow->addWidget(stopButton_);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    connect(runButton_,  &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);

    statusBar()->showMessage("Ready");
}

// -------------------------------------------------------
// Slots -- called when SketchThread emits signals
// -------------------------------------------------------
void MainWindow::onSerialOutput(QString text) {
    // Append without adding an extra newline -- text already has one if needed
    serialMonitor_->moveCursor(QTextCursor::End);
    serialMonitor_->insertPlainText(text);
    serialMonitor_->moveCursor(QTextCursor::End);
}

void MainWindow::onPinChanged(int pin, int value) {
    if (pin == 13) {
        pinStateLabel_->setText(
            QString("Pin 13: %1").arg(value ? "HIGH" : "LOW")
        );
        // Green for HIGH, red for LOW
        pinStateLabel_->setStyleSheet(
            value ? "font-size: 14px; font-weight: bold; padding: 4px; color: #4ec94e;"
                  : "font-size: 14px; font-weight: bold; padding: 4px; color: #e05252;"
        );
    }
}

void MainWindow::onSketchReloaded() {
    serialMonitor_->appendPlainText("\n--- sketch reloaded ---\n");
    statusBar()->showMessage("Sketch hot-reloaded");
}

void MainWindow::onLoadFailed(QString reason) {
    serialMonitor_->appendPlainText("ERROR: " + reason);
    statusBar()->showMessage("Load failed");
    runButton_->setEnabled(true);
    stopButton_->setEnabled(false);
}

// -------------------------------------------------------
// Button handlers
// -------------------------------------------------------
void MainWindow::onRunClicked() {
    QString path = QFileDialog::getOpenFileName(
        this, "Select sketch", QString(), "C++ files (*.cpp *.ino)"
    );
    if (path.isEmpty()) return;

    serialMonitor_->clear();
    statusBar()->showMessage("Compiling: " + path);
    runButton_->setEnabled(false);

    Compiler compiler;
    compiler.set_compiler_path("C:/Qt/Tools/mingw1310_64/bin/g++.exe");
    compiler.set_include_path("C:/Users/cdsto/Documents/VirtualBench");
    compiler.set_output_dir("C:/Users/cdsto/Documents/VirtualBench");

    CompileResult result = compiler.compile(path.toStdString());

    // Always show raw output so we can see what happened
    serialMonitor_->appendPlainText("=== Compiler output ===\n");
    serialMonitor_->appendPlainText(QString::fromStdString(result.raw_output));
    serialMonitor_->appendPlainText(
        result.success ? "\n=== Compile succeeded ===\n"
                       : "\n=== Compile failed ===\n"
    );
    serialMonitor_->appendPlainText(
        "DLL path: " + QString::fromStdString(result.dll_path) + "\n"
    );

    if (!result.success) {
        statusBar()->showMessage("Compile failed");
        runButton_->setEnabled(true);
        return;
    }

    statusBar()->showMessage("Running: " + path);
    stopButton_->setEnabled(true);
    sketchThread_->startSketch(QString::fromStdString(result.dll_path));
}

void MainWindow::onStopClicked() {
    sketchThread_->stopSketch();
    statusBar()->showMessage("Stopped");
    runButton_->setEnabled(true);
    stopButton_->setEnabled(false);
}
