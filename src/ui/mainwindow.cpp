#include "src/ui/mainwindow.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/circuitdetector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <QStatusBar>
#include <QFont>
#include <QLabel>
#include <QFrame>
#include <QCoreApplication>


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("VirtualBench");
    resize(1280, 800);
    setMinimumSize(900, 600);

    QWidget*     central = new QWidget(this);
    QVBoxLayout* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setCentralWidget(central);

    setupToolbar(central, layout);
    setupMainArea(central, layout);

    // Simulation thread
    sketchThread_ = new SketchThread(this);
    connect(sketchThread_, &SketchThread::serialOutput,
            this, &MainWindow::onSerialOutput);
    connect(sketchThread_, &SketchThread::pinChanged,
            this, &MainWindow::onPinChanged);
    connect(sketchThread_, &SketchThread::sketchReloaded,
            this, &MainWindow::onSketchReloaded);
    connect(sketchThread_, &SketchThread::loadFailed,
            this, &MainWindow::onLoadFailed);

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    if (sketchThread_)
        sketchThread_->stopSketch();
}

// -------------------------------------------------------
// Toolbar -- Run, Stop, Open, board label
// -------------------------------------------------------
void MainWindow::setupToolbar(QWidget* parent, QVBoxLayout* layout) {
    QWidget*     toolbar     = new QWidget(parent);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 6);
    toolbarLayout->setSpacing(6);
    toolbar->setStyleSheet(
        "QWidget { background: #1e1e1e; border-bottom: 1px solid #333; }"
    );
    toolbar->setFixedHeight(42);

    // App title
    QLabel* title = new QLabel("VirtualBench", toolbar);
    title->setStyleSheet("color: #cccccc; font-size: 13px; font-weight: bold;"
                         "border: none; background: transparent;");
    toolbarLayout->addWidget(title);

    toolbarLayout->addSpacing(8);

    // Run button
    runButton_ = new QPushButton("Run", toolbar);
    runButton_->setFixedSize(64, 26);
    runButton_->setStyleSheet(
        "QPushButton { background: #2d7a2d; color: #ffffff; border: none;"
        "border-radius: 4px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #3a9a3a; }"
        "QPushButton:disabled { background: #2a2a2a; color: #555; }"
    );
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    toolbarLayout->addWidget(runButton_);

    // Stop button
    stopButton_ = new QPushButton("Stop", toolbar);
    stopButton_->setFixedSize(64, 26);
    stopButton_->setEnabled(false);
    stopButton_->setStyleSheet(
        "QPushButton { background: #7a2d2d; color: #ffffff; border: none;"
        "border-radius: 4px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #9a3a3a; }"
        "QPushButton:disabled { background: #2a2a2a; color: #555; }"
    );
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    toolbarLayout->addWidget(stopButton_);

    // Open sketch button
    QPushButton* openButton = new QPushButton("Open sketch", toolbar);
    openButton->setFixedHeight(26);
    openButton->setStyleSheet(
        "QPushButton { background: transparent; color: #aaaaaa; border: 1px solid #444;"
        "border-radius: 4px; font-size: 12px; padding: 0 10px; }"
        "QPushButton:hover { background: #2a2a2a; color: #ffffff; }"
    );
    connect(openButton, &QPushButton::clicked, this, &MainWindow::onOpenClicked);
    toolbarLayout->addWidget(openButton);

    toolbarLayout->addStretch();

    // Board label
    boardLabel_ = new QLabel("Arduino Uno — ATmega328P", toolbar);
    boardLabel_->setStyleSheet("color: #666; font-size: 11px; border: none;"
                               "background: transparent;");
    toolbarLayout->addWidget(boardLabel_);

    layout->addWidget(toolbar);
}

// -------------------------------------------------------
// Main area -- horizontal splitter: editor | (canvas + debug)
// -------------------------------------------------------
void MainWindow::setupMainArea(QWidget* parent, QVBoxLayout* layout) {
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, parent);
    mainSplitter->setHandleWidth(1);
    mainSplitter->setStyleSheet("QSplitter::handle { background: #333; }");

    // Left -- editor panel
    mainSplitter->addWidget(buildEditorPanel());

    // Right -- vertical splitter: canvas on top, debug on bottom
    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    rightSplitter->setHandleWidth(1);
    rightSplitter->setStyleSheet("QSplitter::handle { background: #333; }");
    rightSplitter->addWidget(buildCanvasPanel());
    rightSplitter->addWidget(buildDebugPanel());
    rightSplitter->setSizes({420, 200});

    mainSplitter->addWidget(rightSplitter);

    // Left panel ~380px, right panel gets the rest
    mainSplitter->setSizes({380, 900});

    layout->addWidget(mainSplitter);
}

// -------------------------------------------------------
// Editor panel (left)
// -------------------------------------------------------
QWidget* MainWindow::buildEditorPanel() {
    QWidget*     panel  = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Panel header
    QLabel* header = new QLabel("  SKETCH EDITOR");
    header->setFixedHeight(24);
    header->setStyleSheet(
        "background: #252526; color: #888; font-size: 10px;"
        "font-weight: bold; border-bottom: 1px solid #333;"
    );
    layout->addWidget(header);

    // Code editor
    codeEditor_ = new QPlainTextEdit();
    codeEditor_->setStyleSheet(
        "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4;"
        "border: none; font-family: 'Courier New', monospace; font-size: 13px; }"
    );
    codeEditor_->setLineWrapMode(QPlainTextEdit::NoWrap);

    // Default starter sketch
    codeEditor_->setPlainText(
        "#include \"src/core/runtime/arduinoapi.h\"\n"
        "#include <string>\n"
        "using namespace vb;\n\n"
        "static ArduinoAPI* api = nullptr;\n"
        "static int counter = 0;\n\n"
        "extern \"C\" __declspec(dllexport)\n"
        "void vb_init(ArduinoAPI* a) { api = a; }\n\n"
        "extern \"C\" __declspec(dllexport)\n"
        "void vb_setup() {\n"
        "    api->Serial_begin(9600);\n"
        "    api->pinMode(13, OUTPUT);\n"
        "}\n\n"
        "extern \"C\" __declspec(dllexport)\n"
        "void vb_loop() {\n"
        "    api->digitalWrite(13, HIGH);\n"
        "    api->delay(1000);\n"
        "    api->digitalWrite(13, LOW);\n"
        "    api->delay(1000);\n"
        "    counter++;\n"
        "    std::string msg = \"Blink #\" + std::to_string(counter);\n"
        "    api->Serial_println(msg.c_str());\n"
        "}\n"
    );

    layout->addWidget(codeEditor_);
    return panel;
}

// -------------------------------------------------------
// Canvas panel (top right) -- placeholder for now
// -------------------------------------------------------
QWidget* MainWindow::buildCanvasPanel() {
    QWidget*     panel  = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel* header = new QLabel("  CIRCUIT CANVAS");
    header->setFixedHeight(24);
    header->setStyleSheet(
        "background: #252526; color: #888; font-size: 10px;"
        "font-weight: bold; border-bottom: 1px solid #333;"
    );
    layout->addWidget(header);

    canvasWidget_ = new CanvasWidget();
    layout->addWidget(canvasWidget_);

    return panel;
}

// -------------------------------------------------------
// Debug panel (bottom right) -- tabbed: serial, timeline, watch
// -------------------------------------------------------
QWidget* MainWindow::buildDebugPanel() {
    QWidget*     panel  = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    debugTabs_ = new QTabWidget();
    debugTabs_->setStyleSheet(
        "QTabWidget::pane { border: none; background: #1e1e1e; }"
        "QTabBar::tab { background: #252526; color: #888; padding: 4px 14px;"
        "font-size: 11px; border: none; border-right: 1px solid #333; }"
        "QTabBar::tab:selected { background: #1e1e1e; color: #cccccc; }"
        "QTabBar::tab:hover { background: #2a2a2a; color: #aaaaaa; }"
    );

    // Tab 1 -- Serial monitor
    serialMonitor_ = new QPlainTextEdit();
    serialMonitor_->setReadOnly(true);
    serialMonitor_->setMaximumBlockCount(2000);
    serialMonitor_->setStyleSheet(
        "QPlainTextEdit { background: #1e1e1e; color: #4ec94e;"
        "border: none; font-family: 'Courier New', monospace; font-size: 12px; }"
    );
    debugTabs_->addTab(serialMonitor_, "Serial monitor");

    // Tab 2 -- Signal timeline (placeholder)
    QLabel* timelinePlaceholder = new QLabel("Signal timeline — coming soon");
    timelinePlaceholder->setAlignment(Qt::AlignCenter);
    timelinePlaceholder->setStyleSheet("background: #1e1e1e; color: #444;");
    debugTabs_->addTab(timelinePlaceholder, "Signal timeline");

    // Tab 3 -- Variable watch (placeholder)
    QLabel* watchPlaceholder = new QLabel("Variable watch — coming soon");
    watchPlaceholder->setAlignment(Qt::AlignCenter);
    watchPlaceholder->setStyleSheet("background: #1e1e1e; color: #444;");
    debugTabs_->addTab(watchPlaceholder, "Variable watch");

    layout->addWidget(debugTabs_);
    return panel;
}

// -------------------------------------------------------
// Slots
// -------------------------------------------------------
void MainWindow::onSerialOutput(QString text) {
    serialMonitor_->moveCursor(QTextCursor::End);
    serialMonitor_->insertPlainText(text);
    serialMonitor_->moveCursor(QTextCursor::End);
}

void MainWindow::onPinChanged(int pin, int value) {
    canvasWidget_->updatePin(pin, value);
    detector_.confirm_pin(pin);
    statusBar()->showMessage(
        QString("Pin %1 -> %2").arg(pin).arg(value ? "HIGH" : "LOW")
    );
}

void MainWindow::onSketchReloaded() {
    serialMonitor_->appendPlainText("\n--- sketch reloaded ---\n");
    statusBar()->showMessage("Sketch hot-reloaded");
}

void MainWindow::onLoadFailed(QString reason) {
    serialMonitor_->appendPlainText("ERROR: " + reason + "\n");
    statusBar()->showMessage("Load failed");
    runButton_->setEnabled(true);
    stopButton_->setEnabled(false);
}

// -------------------------------------------------------
// Button handlers
// -------------------------------------------------------
void MainWindow::onRunClicked() {
    if (currentSketchPath_.isEmpty()) {
        onOpenClicked();
        if (currentSketchPath_.isEmpty()) return;
    }

    serialMonitor_->clear();
    statusBar()->showMessage("Compiling...");
    runButton_->setEnabled(false);

    // Save editor content to disk before compiling
    if (!currentSketchPath_.isEmpty()) {
        QFile file(currentSketchPath_);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(codeEditor_->toPlainText().toUtf8());
            file.close();
        }
    }

    Compiler compiler;
    compiler.set_compiler_path("C:/Qt/Tools/mingw1310_64/bin/g++.exe");
    compiler.set_include_path("C:/Users/cdsto/Documents/VirtualBench");

    // Output DLL to same folder as sketch
    std::string sketch_path = currentSketchPath_.toStdString();
    std::string sketch_dir  = sketch_path;
    size_t slash = sketch_dir.find_last_of("/\\");
    if (slash != std::string::npos)
        sketch_dir = sketch_dir.substr(0, slash);
    compiler.set_output_dir(sketch_dir);

    CompileResult result = compiler.compile(sketch_path);

    if (!result.success) {
        serialMonitor_->appendPlainText("=== Compile errors ===\n");
        serialMonitor_->appendPlainText(QString::fromStdString(result.raw_output));
        statusBar()->showMessage("Compile failed");
        runButton_->setEnabled(true);
        return;
    }

    statusBar()->showMessage("Running: " + currentSketchPath_);
    stopButton_->setEnabled(true);
    sketchThread_->startSketch(QString::fromStdString(result.dll_path));
    
    detector_.detect(codeEditor_->toPlainText().toStdString());
    canvasWidget_->refresh(detector_.components());
}

void MainWindow::onStopClicked() {
    sketchThread_->stopSketch();
    statusBar()->showMessage("Stopped");
    runButton_->setEnabled(true);
    stopButton_->setEnabled(false);
}

void MainWindow::onOpenClicked() {
    QString path = QFileDialog::getOpenFileName(
        this, "Open sketch", QString(), "C++ files (*.cpp *.ino)"
    );
    if (path.isEmpty()) return;

    currentSketchPath_ = path;

    // Load the file content into the editor
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        codeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
        file.close();
    }

    setWindowTitle("VirtualBench — " + QFileInfo(path).fileName());
    statusBar()->showMessage("Opened: " + path);
}