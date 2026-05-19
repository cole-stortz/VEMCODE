#include "src/ui/mainwindow.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/circuitdetector.h"
#include "src/core/build/preprocessor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <QStatusBar>
#include <QFont>
#include <QLabel>
#include <QFrame>
#include <QCoreApplication>
#include <QTextEdit>
#include <QInputDialog>
#include <QDir>

// -------------------------------------------------------
// UI color palette -- change these to retheme the app
// -------------------------------------------------------

// Toolbar
static const QString STYLE_TOOLBAR =
    "QWidget { background: #1e1e1e; border-bottom: 1px solid #333; }";
static const QString STYLE_TITLE =
    "color: #cccccc; font-size: 13px; font-weight: bold;"
    "border: none; background: transparent;";
static const QString STYLE_BOARD_LABEL =
    "color: #666; font-size: 11px; border: none; background: transparent;";

// Buttons
static const QString STYLE_BTN_RUN =
    "QPushButton { background: #2d7a2d; color: #ffffff; border: none;"
    "border-radius: 4px; font-size: 12px; font-weight: bold; }"
    "QPushButton:hover { background: #3a9a3a; }"
    "QPushButton:disabled { background: #2a2a2a; color: #555; }";
static const QString STYLE_BTN_STOP =
    "QPushButton { background: #7a2d2d; color: #ffffff; border: none;"
    "border-radius: 4px; font-size: 12px; font-weight: bold; }"
    "QPushButton:hover { background: #9a3a3a; }"
    "QPushButton:disabled { background: #2a2a2a; color: #555; }";
static const QString STYLE_BTN_OUTLINE =
    "QPushButton { background: transparent; color: #aaaaaa; border: 1px solid #444;"
    "border-radius: 4px; font-size: 12px; padding: 0 10px; }"
    "QPushButton:hover { background: #2a2a2a; color: #ffffff; }";

// Splitter
static const QString STYLE_SPLITTER =
    "QSplitter::handle { background: #333; }";

// Panel headers
static const QString STYLE_PANEL_HEADER =
    "background: #252526; color: #888; font-size: 10px;"
    "font-weight: bold; border-bottom: 1px solid #333;";

// Code editor
static const QString STYLE_EDITOR =
    "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4;"
    "border: none; font-family: 'Courier New', monospace; font-size: 13px; }";

// Serial monitor
static const QString STYLE_SERIAL =
    "QPlainTextEdit { background: #1e1e1e; color: #4ec94e;"
    "border: none; font-family: 'Courier New', monospace; font-size: 12px; }";

// Debug tab bar
static const QString STYLE_TABS =
    "QTabWidget::pane { border: none; background: #1e1e1e; }"
    "QTabBar::tab { background: #252526; color: #888; padding: 4px 14px;"
    "font-size: 11px; border: none; border-right: 1px solid #333; }"
    "QTabBar::tab:selected { background: #1e1e1e; color: #cccccc; }"
    "QTabBar::tab:hover { background: #2a2a2a; color: #aaaaaa; }";


// Error highlight
static const QColor COLOR_ERROR_BG("#3a0000");

// -------------------------------------------------------

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
    setupMainArea(central, layout);  // variableWatch_ gets created here

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
    connect(sketchThread_, &SketchThread::variableChanged,  
            variableWatch_, &VariableWatch::onVariableChanged);

    // Ctrl+S to save
    QShortcut* save_shortcut = new QShortcut(QKeySequence::Save, this);
    connect(save_shortcut, &QShortcut::activated, this, &MainWindow::onSaveClicked);

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    if (sketchThread_)
        sketchThread_->stopSketch();
}

// -------------------------------------------------------
// Toolbar -- Run, Stop, Open, Save, board label
// -------------------------------------------------------
void MainWindow::setupToolbar(QWidget* parent, QVBoxLayout* layout) {
    QWidget*     toolbar      = new QWidget(parent);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 6);
    toolbarLayout->setSpacing(6);
    toolbar->setStyleSheet(STYLE_TOOLBAR);
    toolbar->setFixedHeight(42);

    QLabel* title = new QLabel("VirtualBench", toolbar);
    title->setStyleSheet(STYLE_TITLE);
    toolbarLayout->addWidget(title);
    toolbarLayout->addSpacing(8);

    runButton_ = new QPushButton("Run", toolbar);
    runButton_->setFixedSize(64, 26);
    runButton_->setStyleSheet(STYLE_BTN_RUN);
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    toolbarLayout->addWidget(runButton_);

    stopButton_ = new QPushButton("Stop", toolbar);
    stopButton_->setFixedSize(64, 26);
    stopButton_->setEnabled(false);
    stopButton_->setStyleSheet(STYLE_BTN_STOP);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    toolbarLayout->addWidget(stopButton_);

    QPushButton* openButton = new QPushButton("Open sketch", toolbar);
    openButton->setFixedHeight(26);
    openButton->setStyleSheet(STYLE_BTN_OUTLINE);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::onOpenClicked);
    toolbarLayout->addWidget(openButton);

    QPushButton* saveButton = new QPushButton("Save sketch", toolbar);
    saveButton->setFixedHeight(26);
    saveButton->setStyleSheet(STYLE_BTN_OUTLINE);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    toolbarLayout->addWidget(saveButton);

    toolbarLayout->addStretch();

    boardLabel_ = new QLabel("Arduino Uno — ATmega328P", toolbar);
    boardLabel_->setStyleSheet(STYLE_BOARD_LABEL);
    toolbarLayout->addWidget(boardLabel_);

    layout->addWidget(toolbar);
}

// -------------------------------------------------------
// Main area -- horizontal splitter: editor | (canvas + debug)
// -------------------------------------------------------
void MainWindow::setupMainArea(QWidget* parent, QVBoxLayout* layout) {
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, parent);
    mainSplitter->setHandleWidth(1);
    mainSplitter->setStyleSheet(STYLE_SPLITTER);
    mainSplitter->addWidget(buildEditorPanel());

    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    rightSplitter->setHandleWidth(1);
    rightSplitter->setStyleSheet(STYLE_SPLITTER);
    rightSplitter->addWidget(buildCanvasPanel());
    rightSplitter->addWidget(buildDebugPanel());
    rightSplitter->setSizes({420, 200});

    mainSplitter->addWidget(rightSplitter);
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

    QLabel* header = new QLabel("  SKETCH EDITOR");
    header->setFixedHeight(24);
    header->setStyleSheet(STYLE_PANEL_HEADER);
    layout->addWidget(header);

    codeEditor_ = new EditorWithLines();
    highlighter_ = new CodeHighlighter(codeEditor_->document());
    codeEditor_->setStyleSheet(STYLE_EDITOR);
    codeEditor_->setLineWrapMode(QPlainTextEdit::NoWrap);

    // Tab width = 4 spaces
    QFontMetrics metrics(codeEditor_->font());
    codeEditor_->setTabStopDistance(4 * metrics.horizontalAdvance(' '));
    codeEditor_->installEventFilter(this);

    lineNumbers_ = new LineNumberArea(codeEditor_);
    lineNumbers_->show();

    // Default starter sketch
    codeEditor_->setPlainText(
        "#define LED_PIN 13\n\n"
        "void setup() {\n"
        "    Serial.begin(9600);\n"
        "    pinMode(LED_PIN, OUTPUT);\n"
        "    Serial.println(\"VirtualBench ready\");\n"
        "}\n\n"
        "void loop() {\n"
        "    digitalWrite(LED_PIN, HIGH);\n"
        "    delay(1000);\n"
        "    digitalWrite(LED_PIN, LOW);\n"
        "    delay(1000);\n"
        "    Serial.println(\"Blink\");\n"
        "}\n"
    );

    layout->addWidget(codeEditor_);
    return panel;
}

// -------------------------------------------------------
// Canvas panel (top right)
// -------------------------------------------------------
QWidget* MainWindow::buildCanvasPanel() {
    QWidget*     panel  = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel* header = new QLabel("  CIRCUIT CANVAS");
    header->setFixedHeight(24);
    header->setStyleSheet(STYLE_PANEL_HEADER);
    layout->addWidget(header);

    canvasWidget_ = new CanvasWidget();
    layout->addWidget(canvasWidget_);

    // Wire button clicks on canvas to pin injection in simulation
    connect(canvasWidget_, &CanvasWidget::buttonPressed,
            this, [this](int pin, int value) {
                sketchThread_->injectPin(pin, value);
            });

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
    debugTabs_->setStyleSheet(STYLE_TABS);

    serialMonitor_ = new QPlainTextEdit();
    serialMonitor_->setReadOnly(true);
    serialMonitor_->setMaximumBlockCount(2000);
    serialMonitor_->setStyleSheet(STYLE_SERIAL);
    debugTabs_->addTab(serialMonitor_, "Serial monitor");

    signalTimeline_ = new SignalTimeline();
    debugTabs_->addTab(signalTimeline_, "Signal timeline");

    variableWatch_ = new VariableWatch();
    debugTabs_->addTab(variableWatch_, "Variable watch");

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
    signalTimeline_->addEvent(pin, value, simTimer_.elapsed());
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
    // If no file is open, save editor content to a temp file and run that
    if (currentSketchPath_.isEmpty()) {
        QString temp_path = QDir::tempPath() + "/vb_sketch.cpp";
        QFile temp_file(temp_path);
        if (temp_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            temp_file.write(codeEditor_->toPlainText().toUtf8());
            temp_file.close();
        }
        currentSketchPath_ = temp_path;
        setWindowTitle("VirtualBench — unsaved sketch");
    }

    serialMonitor_->clear();
    statusBar()->showMessage("Compiling...");
    runButton_->setEnabled(false);

    // Save editor content to disk before compiling
    QFile file(currentSketchPath_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(codeEditor_->toPlainText().toUtf8());
        file.close();
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
        QString raw = QString::fromStdString(result.raw_output);

        QString temp_file_path = QString::fromStdString(sketch_dir + "/_vb_temp.cpp");

        QRegularExpression path_re(
            QRegularExpression::escape(temp_file_path + ":") + "(?=(\\d))"
        );
        raw.replace(path_re, "line ");

        QRegularExpression context_re(
            QRegularExpression::escape(temp_file_path + ":")
        );
        raw.replace(context_re, "");

        // Adjust line numbers -- subtract injected header offset
        QRegularExpression line_num_re(R"(line (\d+):)");
        QString adjusted_raw;
        int last_pos = 0;
        QRegularExpressionMatchIterator it = line_num_re.globalMatch(raw);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int raw_line = match.captured(1).toInt();
            int adj_line = raw_line - Preprocessor::INJECTED_HEADER_LINES;
            adjusted_raw += raw.mid(last_pos, match.capturedStart() - last_pos);
            adjusted_raw += QString("line %1:").arg(qMax(1, adj_line));
            last_pos = match.capturedEnd();
        }
        adjusted_raw += raw.mid(last_pos);
        raw = adjusted_raw;

        // Strip gcc source context line numbers (e.g. "16 | api->pinMode...")
        QRegularExpression context_line_re(R"(\n\s*\d+\s*\|)");
        raw.replace(context_line_re, "\n   |");

        // Restore Serial.method() names -- preprocessor converted dots to underscores
        raw.replace("Serial_println(",  "Serial.println(");
        raw.replace("Serial_print(",    "Serial.print(");
        raw.replace("Serial_begin(",    "Serial.begin(");
        raw.replace("Serial_available(","Serial.available(");
        raw.replace("Serial_read(",     "Serial.read(");

        // Strip api-> -- user never wrote this
        raw.replace("api->", "");

        serialMonitor_->appendPlainText("=== Compile errors ===\n");
        serialMonitor_->appendPlainText(raw);
        statusBar()->showMessage("Compile failed");
        runButton_->setEnabled(true);
        showCompileErrors(result);
        return;
    }

    clearCompileErrors();
    signalTimeline_->clear();
    variableWatch_->clear();
    simTimer_.start();
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
    QString sketches_root = QCoreApplication::applicationDirPath() + "/sketches";
    QString path = QFileDialog::getOpenFileName(
        this, "Open sketch", sketches_root, "C++ files (*.cpp *.ino)"
    );
    if (path.isEmpty()) return;

    currentSketchPath_ = path;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        codeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
        file.close();
    }

    setWindowTitle("VirtualBench — " + QFileInfo(path).fileName());
    statusBar()->showMessage("Opened: " + path);
}

void MainWindow::onSaveClicked() {
    bool ok;
    QString name = QInputDialog::getText(
        this, "Save sketch", "Sketch name:",
        QLineEdit::Normal, "my_sketch", &ok
    );
    if (!ok || name.trimmed().isEmpty()) return;

    name = name.trimmed().replace(" ", "_");

    QString sketches_root = QCoreApplication::applicationDirPath() + "/sketches";
    QString sketch_dir    = sketches_root + "/" + name;
    QDir().mkpath(sketch_dir);

    QString file_path = sketch_dir + "/" + name + ".cpp";
    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to save: " + file_path);
        return;
    }
    file.write(codeEditor_->toPlainText().toUtf8());
    file.close();

    // Update current path so Run compiles this file going forward
    currentSketchPath_ = file_path;
    setWindowTitle("VirtualBench — " + name + ".cpp");
    statusBar()->showMessage("Saved: " + file_path);
}

// -------------------------------------------------------
// Editor helpers
// -------------------------------------------------------
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == codeEditor_ && event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Tab) {
            codeEditor_->insertPlainText("    ");
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::clearCompileErrors() {
    codeEditor_->setExtraSelections({});
}

void MainWindow::showCompileErrors(const CompileResult& result) {
    clearCompileErrors();

    QList<QTextEdit::ExtraSelection> selections;

    for (const auto& err : result.errors) {
        if (!err.is_error) continue;

        int adjusted_line = err.line - Preprocessor::INJECTED_HEADER_LINES;
        if (adjusted_line < 1) continue;
        QTextBlock block = codeEditor_->document()->findBlockByLineNumber(
            adjusted_line - 1);
        if (!block.isValid()) continue;

        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(COLOR_ERROR_BG);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.format.setToolTip(QString::fromStdString(err.message));
        sel.cursor = QTextCursor(block);
        sel.cursor.select(QTextCursor::LineUnderCursor);
        selections.append(sel);
    }

    codeEditor_->setExtraSelections(selections);

    // Move cursor to first error line
    for (const auto& err : result.errors) {
        if (err.is_error) {
            int adjusted_line = err.line - Preprocessor::INJECTED_HEADER_LINES;
            if (adjusted_line < 1) continue;
            QTextBlock block = codeEditor_->document()
                ->findBlockByLineNumber(adjusted_line - 1);
            if (block.isValid()) {
                codeEditor_->setTextCursor(QTextCursor(block));
                codeEditor_->centerCursor();
                break;
            }
        }
    }
}

