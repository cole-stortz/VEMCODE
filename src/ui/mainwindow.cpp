#include "src/ui/mainwindow.h"
#include "src/ui/canvaswidget.h"
#include <regex>
#include <algorithm>
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
#include <QMenu>
#include <QAction>
#include <QToolTip>

// Returns true if a define/const name looks like a pin definition
static bool is_pin_name(const std::string& name) {
    static const char* INCLUDE[] = {
        "PIN", "LED", "BTN", "BUTTON", "SERVO", "TRIG", "ECHO",
        "SENSOR", "MOTOR", "CLK", "DT", "CS", "RST", "RS",
        "SWITCH", "BUZZER", "SPEAKER", "PIEZO", "POT", "DIAL"
    };
    static const char* EXCLUDE[] = {
        "COUNT", "NUM", "SIZE", "LEN", "MAX", "SPEED", "BAUD", "RATE", "DELAY", "TIMEOUT"
    };
    std::string up = name;
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    for (const char* kw : EXCLUDE)
        if (up.find(kw) != std::string::npos) return false;
    for (const char* kw : INCLUDE)
        if (up.find(kw) != std::string::npos) return true;
    return false;
}

// PWM-capable pins per board; empty = board supports PWM on all pins (skip check)
static std::vector<int> get_pwm_pins_for(const BoardProfile& p) {
    std::string name = p.name;
    if (name == "Arduino Uno" || name == "Arduino Nano")
        return {3, 5, 6, 9, 10, 11};
    if (name == "Arduino Mega 2560")
        return {2,3,4,5,6,7,8,9,10,11,12,13,44,45,46};
    return {}; // Due / Teensy: all pins support PWM
}

// Rewrite cryptic GCC error messages to plain English
static QString humanizeErrors(const QString& raw) {
    struct Rule { QRegularExpression re; QString repl; };
    static const Rule rules[] = {
        { QRegularExpression(R"('([\w:<>*& ]+)' was not declared in this scope)"),
          "'\\1' not found — did you forget to declare it?" },
        { QRegularExpression(R"(no matching function for call to '([^']+)')"),
          "Wrong arguments passed to '\\1'" },
        { QRegularExpression(R"(expected ';' before '}' token)"),
          "Missing semicolon, probably the line above" },
        { QRegularExpression(R"(expected '}' at end of input)"),
          "Unclosed brace — one of your { was never closed" },
        { QRegularExpression(R"(lvalue required as left operand of assignment)"),
          "Did you mean == instead of =?" },
        { QRegularExpression(R"(undefined reference to [`']([^'`]+)[`'])"),
          "Function '\\1' is used but never defined" },
        { QRegularExpression(R"(control reaches end of non-void function)"),
          "Function is missing a return statement" },
        { QRegularExpression(R"(expected unqualified-id before '\{' token)"),
          "Code found outside a function — all code must be inside setup(), loop(), or another function" },
        { QRegularExpression(R"(too (many|few) arguments to function '([^']+)')"),
          "Wrong number of arguments passed to '\\2'" },
        { QRegularExpression(R"(stray '\\[^']*' in program)"),
          "Invalid character in code — this sometimes happens when copy-pasting from a website; try retyping the line" },
        { QRegularExpression(R"(overflow in implicit constant conversion)"),
          "Number is too large for this variable type — try using long instead of int" },
        { QRegularExpression(R"(comparison between pointer and integer)"),
          "Can't compare strings with == — use strcmp() or the String class" },
    };
    QString result = raw;
    for (const auto& r : rules)
        result.replace(r.re, r.repl);
    return result;
}

// UI color palette

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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("VEMCODE");
    resize(1280, 720);
    setMinimumSize(900, 600);

    QWidget*     central = new QWidget(this);
    QVBoxLayout* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setCentralWidget(central);

    QSettings settings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);
    compilerPath_ = settings.value("compiler/path", "").toString();
    projectRoot_  = settings.value("compiler/project_root", "").toString();

    QString boardName = settings.value("board/name", "Arduino Uno").toString();
    if      (boardName == "Arduino Nano")       activeProfile_ = BOARD_NANO;
    else if (boardName == "Arduino Mega 2560")  activeProfile_ = BOARD_MEGA;
    else if (boardName == "Arduino Due")        activeProfile_ = BOARD_DUE;
    else if (boardName == "Teensy 4.1")         activeProfile_ = BOARD_TEENSY;
    else                                        activeProfile_ = BOARD_UNO;

    if (compilerPath_.isEmpty() || projectRoot_.isEmpty()) {
        SettingsDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            compilerPath_ = dialog.compilerPath();
            projectRoot_  = dialog.projectRoot();
            settings.setValue("compiler/path", compilerPath_);
            settings.setValue("compiler/project_root", projectRoot_);
        }
    }

    setupToolbar(central, layout);
    boardLabel_->setText(activeProfile_.name);

    setupMainArea(central, layout);  // variableWatch_ must exist before the connect below
    canvasWidget_->setProfile(activeProfile_);

    sketchThread_ = new SketchThread(this);
    sketchThread_->setProfile(activeProfile_);
    analogNoise_ = settings.value("simulation/analog_noise", false).toBool();
    sketchThread_->setAnalogNoise(analogNoise_);
    connect(sketchThread_, &SketchThread::serialOutput,
            this, &MainWindow::onSerialOutput);
    connect(sketchThread_, &SketchThread::serial1Output,
            this, &MainWindow::onSerial1Output);
    connect(sketchThread_, &SketchThread::serial2Output,
            this, &MainWindow::onSerial2Output);
    connect(sketchThread_, &SketchThread::softSerialOutput,
            this, &MainWindow::onSoftSerialOutput);
    connect(sketchThread_, &SketchThread::pinChanged,
            this, &MainWindow::onPinChanged);
    connect(sketchThread_, &SketchThread::sketchReloaded,
            this, &MainWindow::onSketchReloaded);
    connect(sketchThread_, &SketchThread::loadFailed,
            this, &MainWindow::onLoadFailed);
    connect(sketchThread_, &SketchThread::sketchCrashed,
            this, [this](const QString& reason) {
                serialMonitor_->appendPlainText("ERROR: " + reason + "\n");
                statusBar()->showMessage("Sketch crashed");
                runButton_->setEnabled(true);
                stopButton_->setEnabled(false);
            });
    connect(sketchThread_, &SketchThread::variableChanged,
            variableWatch_, &VariableWatch::onVariableChanged);
    connect(sketchThread_, &SketchThread::lcdPrint,
            canvasWidget_, &CanvasWidget::updateLcdText);

    QShortcut* save_shortcut = new QShortcut(QKeySequence::Save, this);
    connect(save_shortcut, &QShortcut::activated, this, &MainWindow::onSaveClicked);

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    if (sketchThread_)
        sketchThread_->stopSketch();
}

// Toolbar -- Run, Stop, Open, Save, board label
void MainWindow::setupToolbar(QWidget* parent, QVBoxLayout* layout) {
    QWidget*     toolbar      = new QWidget(parent);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 6);
    toolbarLayout->setSpacing(6);
    toolbar->setStyleSheet(STYLE_TOOLBAR);
    toolbar->setFixedHeight(42);

    QLabel* title = new QLabel("VEMCODE", toolbar);
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

    QLabel* speedLabel = new QLabel("Speed:", toolbar);
    speedLabel->setStyleSheet("color: #888; font-size: 11px; border: none; background: transparent;");
    toolbarLayout->addWidget(speedLabel);

    speedSlider_ = new QSlider(Qt::Horizontal, toolbar);
    speedSlider_->setRange(1, 25);
    speedSlider_->setValue(10);
    speedSlider_->setFixedWidth(80);
    speedSlider_->setToolTip("Simulation speed (5 = normal)");
    speedSlider_->setStyleSheet(
        "QSlider::groove:horizontal { background: #333; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #888; width: 12px; height: 12px;"
        "margin: -4px 0; border-radius: 6px; }"
        "QSlider::handle:horizontal:hover { background: #aaa; }"
    );
    connect(speedSlider_, &QSlider::valueChanged, this, &MainWindow::onSpeedChanged);
    toolbarLayout->addWidget(speedSlider_);

    QPushButton* newsketchButton = new QPushButton("New Sketch", toolbar);
    newsketchButton->setFixedHeight(26);
    newsketchButton->setStyleSheet(STYLE_BTN_OUTLINE);
    connect(newsketchButton, &QPushButton::clicked, this, &MainWindow::onNewSketch);
    toolbarLayout->addWidget(newsketchButton);

    QPushButton* openButton = new QPushButton("Open Sketch", toolbar);
    openButton->setFixedHeight(26);
    openButton->setStyleSheet(STYLE_BTN_OUTLINE);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::onOpenClicked);
    toolbarLayout->addWidget(openButton);

    QPushButton* recentButton = new QPushButton("Recent", toolbar);
    recentButton->setFixedHeight(26);
    recentButton->setStyleSheet(STYLE_BTN_OUTLINE);
    connect(recentButton, &QPushButton::clicked, this, &MainWindow::onRecentSketches);
    toolbarLayout->addWidget(recentButton);

    QPushButton* saveButton = new QPushButton("Save Sketch", toolbar);
    saveButton->setFixedHeight(26);
    saveButton->setStyleSheet(STYLE_BTN_OUTLINE);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    toolbarLayout->addWidget(saveButton);

    QPushButton* settingsButton = new QPushButton("Settings", toolbar);
    settingsButton->setFixedHeight(26);
    settingsButton->setStyleSheet(STYLE_BTN_OUTLINE);
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    toolbarLayout->addWidget(settingsButton);

    toolbarLayout->addStretch();

    boardLabel_ = new QLabel("", toolbar);
    boardLabel_->setStyleSheet(STYLE_BOARD_LABEL);
    toolbarLayout->addWidget(boardLabel_);

    layout->addWidget(toolbar);
}

// Main area -- horizontal splitter: editor | (canvas + debug)
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
    rightSplitter->setSizes({300, 220});

    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setSizes({380, 900});

    layout->addWidget(mainSplitter);
}

// Editor panel (left)
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

    codeEditor_->setPlainText(
        "#define LED_PIN 13\n\n"
        "void setup() {\n"
        "    Serial.begin(9600);\n"
        "    pinMode(LED_PIN, OUTPUT);\n"
        "    Serial.println(\"VEMCODE ready\");\n"
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

// Canvas panel (top right)
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

    connect(canvasWidget_, &CanvasWidget::potentiometerChanged,
        this, [this](int pin, int value) {
            sketchThread_->injectAnalog(pin, value);
        });

    connect(canvasWidget_, &CanvasWidget::buttonPressed,
            this, [this](int pin, int value) {
                sketchThread_->injectPin(pin, value);
            });

    connect(canvasWidget_, &CanvasWidget::buttonBounced,
            this, [this](int pin, int value) {
                sketchThread_->injectButtonBounce(pin, value);
            });

    connect(canvasWidget_, &CanvasWidget::pulseInjected,
        this, [this](int pin, unsigned long micros) {
            sketchThread_->injectPulseDuration(pin, micros);
        });

    connect(canvasWidget_, &CanvasWidget::analogInjected,
        this, [this](int pin, int value) {
            sketchThread_->injectAnalog(pin, value);
        });

    connect(canvasWidget_, &CanvasWidget::colorInjected,
        this, [this](int out_pin, int s2_pin, int s3_pin, int r, int g, int b) {
            sketchThread_->injectColor(out_pin, s2_pin, s3_pin, r, g, b);
        });

    return panel;
}

// Serial panel -- extracted so it can be rebuilt on board profile change
QWidget* MainWindow::buildSerialPanel() {
    QWidget*     serialPanel  = new QWidget();
    QVBoxLayout* serialLayout = new QVBoxLayout(serialPanel);
    serialLayout->setContentsMargins(0, 0, 0, 0);
    serialLayout->setSpacing(0);

    serialMonitor_  = new QPlainTextEdit();
    serialMonitor1_ = nullptr;
    serialMonitor2_ = nullptr;
    serialMonitor_->setReadOnly(true);
    serialMonitor_->setMaximumBlockCount(2000);
    serialMonitor_->setStyleSheet(STYLE_SERIAL);

    int serialCount = activeProfile_.serial_count;

    if (serialCount >= 2) {
        QSplitter* serialSplitter = new QSplitter(Qt::Horizontal, serialPanel);
        serialSplitter->setHandleWidth(1);
        serialSplitter->setStyleSheet(STYLE_SPLITTER);

        static const QString STYLE_PORT_LABEL =
            "background: #252526; color: #666; font-size: 10px;"
            "padding-left: 4px; border-bottom: 1px solid #333;";
        static const QString STYLE_PORT_LABEL_BORDERED =
            "background: #252526; color: #666; font-size: 10px;"
            "padding-left: 4px; border-left: 1px solid #333; border-bottom: 1px solid #333;";

        auto makePane = [&](QPlainTextEdit* mon, const QString& label, bool bordered) {
            QWidget*     pane   = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(pane);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            QLabel* lbl = new QLabel(label, pane);
            lbl->setFixedHeight(18);
            lbl->setStyleSheet(bordered ? STYLE_PORT_LABEL_BORDERED : STYLE_PORT_LABEL);
            layout->addWidget(lbl);
            layout->addWidget(mon);
            return pane;
        };

        serialSplitter->addWidget(makePane(serialMonitor_, "  Serial", false));

        serialMonitor1_ = new QPlainTextEdit();
        serialMonitor1_->setReadOnly(true);
        serialMonitor1_->setMaximumBlockCount(2000);
        serialMonitor1_->setStyleSheet(STYLE_SERIAL);
        serialSplitter->addWidget(makePane(serialMonitor1_, "  Serial1", true));

        if (serialCount >= 3) {
            serialMonitor2_ = new QPlainTextEdit();
            serialMonitor2_->setReadOnly(true);
            serialMonitor2_->setMaximumBlockCount(2000);
            serialMonitor2_->setStyleSheet(STYLE_SERIAL);
            serialSplitter->addWidget(makePane(serialMonitor2_, "  Serial2", true));
        }

        serialLayout->addWidget(serialSplitter);
    } else {
        serialLayout->addWidget(serialMonitor_);
    }

    QWidget*     inputRow    = new QWidget();
    QHBoxLayout* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(4, 4, 4, 4);
    inputLayout->setSpacing(4);
    inputRow->setStyleSheet("background: #252526; border-top: 1px solid #333;");
    inputRow->setFixedHeight(36);

    serialInput_ = new QLineEdit();
    serialInput_->setPlaceholderText("Send serial input...");
    serialInput_->setStyleSheet(
        "QLineEdit { background: #1e1e1e; color: #d4d4d4; border: 1px solid #444;"
        "border-radius: 3px; padding: 3px 6px; font-family: 'Courier New'; font-size: 12px; }"
    );
    inputLayout->addWidget(serialInput_);

    QPushButton* sendButton = new QPushButton("Send", inputRow);
    sendButton->setFixedWidth(50);
    sendButton->setStyleSheet(STYLE_BTN_OUTLINE);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSerialSend);
    connect(serialInput_, &QLineEdit::returnPressed, this, &MainWindow::onSerialSend);
    inputLayout->addWidget(sendButton);

    serialLayout->addWidget(inputRow);
    return serialPanel;
}

void MainWindow::rebuildSerialMonitors() {
    int savedTab = debugTabs_->currentIndex();
    debugTabs_->removeTab(0);
    debugTabs_->insertTab(0, buildSerialPanel(), "Serial monitor");
    debugTabs_->setCurrentIndex(savedTab);
}

// Debug panel (bottom right) -- tabbed: serial, timeline, watch
QWidget* MainWindow::buildDebugPanel() {
    QWidget*     panel  = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    debugTabs_ = new QTabWidget();
    debugTabs_->setMinimumHeight(100);
    debugTabs_->setStyleSheet(STYLE_TABS);

    debugTabs_->addTab(buildSerialPanel(), "Serial monitor");
    signalTimeline_ = new SignalTimeline();
    debugTabs_->addTab(signalTimeline_, "Signal timeline");
    variableWatch_ = new VariableWatch();
    debugTabs_->addTab(variableWatch_, "Variable watch");

    layout->addWidget(debugTabs_);
    return panel;
}

// Slots
void MainWindow::onSerialOutput(QString text) {
    serialMonitor_->moveCursor(QTextCursor::End);
    serialMonitor_->insertPlainText(text);
    serialMonitor_->moveCursor(QTextCursor::End);
}

void MainWindow::onSerial1Output(QString text) {
    if (!serialMonitor1_) return;
    serialMonitor1_->moveCursor(QTextCursor::End);
    serialMonitor1_->insertPlainText(text);
    serialMonitor1_->moveCursor(QTextCursor::End);
}

void MainWindow::onSerial2Output(QString text) {
    if (!serialMonitor2_) return;
    serialMonitor2_->moveCursor(QTextCursor::End);
    serialMonitor2_->insertPlainText(text);
    serialMonitor2_->moveCursor(QTextCursor::End);
}

void MainWindow::onSoftSerialOutput(int rxPin, QString text) {
    bool atStart = !softSerialLineStart_.contains(rxPin) || softSerialLineStart_[rxPin];
    QString out;
    for (QChar c : text) {
        if (atStart) {
            out += QString("[SW:%1] ").arg(rxPin);
            atStart = false;
        }
        out += c;
        if (c == '\n') atStart = true;
    }
    softSerialLineStart_[rxPin] = atStart;
    serialMonitor_->moveCursor(QTextCursor::End);
    serialMonitor_->insertPlainText(out);
    serialMonitor_->moveCursor(QTextCursor::End);
}

void MainWindow::onPinChanged(int pin, int value) {
    canvasWidget_->updatePin(pin, value);
    // Mark this pin as actively driven so the detector can distinguish
    // components actually wired up from ones only referenced in source
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

// Button handlers
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
        setWindowTitle("VEMCODE — unsaved sketch");
    }

    serialMonitor_->clear();
    if (serialMonitor1_) serialMonitor1_->clear();
    if (serialMonitor2_) serialMonitor2_->clear();
    statusBar()->showMessage("Compiling...");
    runButton_->setEnabled(false);

    QFile file(currentSketchPath_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(codeEditor_->toPlainText().toUtf8());
        file.close();
    }

    Compiler compiler;
    compiler.set_compiler_path(compilerPath_.toStdString());
    compiler.set_include_path(projectRoot_.toStdString());

    // Output DLL to same folder as sketch
    std::string sketch_path = currentSketchPath_.toStdString();
    std::string sketch_dir  = sketch_path;
    size_t slash = sketch_dir.find_last_of("/\\");
    if (slash != std::string::npos)
        sketch_dir = sketch_dir.substr(0, slash);
    compiler.set_output_dir(sketch_dir);

    // Static pre-compile checks (pin range, PWM, ISR issues, etc.)
    for (const QString& w : runStaticChecks(codeEditor_->toPlainText()))
        serialMonitor_->appendPlainText(w + "\n");

    CompileResult result = compiler.compile(sketch_path);

    // Show pre-compile warnings (unsupported libraries, etc.) regardless of outcome
    for (const auto& w : result.warnings)
        serialMonitor_->appendPlainText(QString::fromStdString(w) + "\n");

    // Apply board hint from // @board comment if present
    if (!result.board_hint.empty()) {
        QString boardName = QString::fromStdString(result.board_hint);
        int oldSerialCount = activeProfile_.serial_count;
        if      (boardName == "Arduino Nano")       activeProfile_ = BOARD_NANO;
        else if (boardName == "Arduino Mega 2560")  activeProfile_ = BOARD_MEGA;
        else if (boardName == "Arduino Due")        activeProfile_ = BOARD_DUE;
        else if (boardName == "Teensy 4.1")         activeProfile_ = BOARD_TEENSY;
        else if (boardName == "Arduino Uno")        activeProfile_ = BOARD_UNO;
        else {
            serialMonitor_->appendPlainText(
                "WARNING: Unknown board '" + boardName + "' in @board hint — using currently selected board instead.\n");
            return;
        }

        canvasWidget_->setProfile(activeProfile_);
        boardLabel_->setText(activeProfile_.name);
        if (sketchThread_) sketchThread_->setProfile(activeProfile_);

        if (activeProfile_.serial_count != oldSerialCount)
            rebuildSerialMonitors();

        QSettings settings(
            QCoreApplication::applicationDirPath() + "/settings.ini",
            QSettings::IniFormat);
        settings.setValue("board/name", boardName);
    }

    if (!result.success) {
        // GCC output references internal temp file paths and the preprocessor's
        // injected header lines -- clean it up before showing it to the user
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

        // Subtract the lines the preprocessor injected above the user's code
        // (Arduino API wrapper, Serial macros, etc.) so errors point to the right line
        QRegularExpression line_num_re(R"(line (\d+):)");
        QString adjusted_raw;
        int last_pos = 0;
        QRegularExpressionMatchIterator it = line_num_re.globalMatch(raw);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int raw_line = match.captured(1).toInt();
            int adj_line = raw_line - result.header_lines;
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

        raw = humanizeErrors(raw);
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

    // Same-pin-claimed-by-two-components warnings
    for (const auto& w : detector_.warnings())
        serialMonitor_->appendPlainText(QString::fromStdString(w) + "\n");

    // No-components-detected hints
    const auto& comps = detector_.components();
    bool has_non_serial = std::any_of(comps.begin(), comps.end(),
        [](const DetectedComponent& c) { return c.type != ComponentType::Serial; });
    if (!has_non_serial) {
        std::string src = codeEditor_->toPlainText().toStdString();
        bool has_pin_defines = false;
        std::regex pin_def_re(R"(#\s*define\s+(\w+)\s+\d+)");
        for (auto it = std::sregex_iterator(src.begin(), src.end(), pin_def_re);
             it != std::sregex_iterator(); ++it) {
            if (is_pin_name((*it)[1].str())) { has_pin_defines = true; break; }
        }
        bool has_hardcoded = std::regex_search(src,
            std::regex(R"((?:digitalWrite|analogWrite|pinMode)\s*\(\s*\d+)"));
        bool has_local_header = std::regex_search(src,
            std::regex(R"(#include\s+"[^"]+\.h")"));

        if (has_pin_defines) {
            serialMonitor_->appendPlainText(
                "HINT: Pin definitions found but couldn't identify component types — "
                "try descriptive names like LED_PIN, SERVO_PIN, BUTTON_PIN\n");
        } else if (has_hardcoded) {
            serialMonitor_->appendPlainText(
                "HINT: Pin numbers are hardcoded — give them names like "
                "'const int LED_PIN = 5;' so the simulator can identify them\n");
        } else if (has_local_header) {
            serialMonitor_->appendPlainText(
                "HINT: No components detected — if your pin definitions are in a header file, "
                "try moving them into the main sketch\n");
        }
    }
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

    setWindowTitle("VEMCODE — " + QFileInfo(path).fileName());
    statusBar()->showMessage("Opened: " + path);
    addToRecentSketches(path);
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
    setWindowTitle("VEMCODE — " + name + ".cpp");
    statusBar()->showMessage("Saved: " + file_path);
    addToRecentSketches(file_path);
}

// Editor helpers
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == codeEditor_ && event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);

        if (key->key() == Qt::Key_Tab) {
            codeEditor_->insertPlainText("    ");
            return true;
        }

        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            QTextCursor cursor = codeEditor_->textCursor();

            cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
            QString line = cursor.selectedText();

            int spaces = 0;
            for (QChar c : line) {
                if (c == ' ') spaces++;
                else break;
            }

            QString trimmed = line.trimmed();
            if (trimmed.endsWith('{'))
                spaces += 4;

            cursor = codeEditor_->textCursor();
            cursor.insertText("\n" + QString(spaces, ' '));
            codeEditor_->setTextCursor(cursor);
            return true;
        }

        // Auto-dedent -- when } is typed on a line with only spaces, remove one indent level
        if (key->key() == Qt::Key_BraceRight) {
            QTextCursor cursor = codeEditor_->textCursor();

            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            QString line = cursor.selectedText();

            if (line.trimmed().isEmpty() && line.length() > 0) {
                int new_indent = qMax(0, (int)line.length() - 4);
                cursor.removeSelectedText();
                cursor.insertText(QString(new_indent, ' ') + "}");
                codeEditor_->setTextCursor(cursor);
                return true;
            }
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

        int adjusted_line = err.line - result.header_lines;
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

    for (const auto& err : result.errors) {
        if (err.is_error) {
            int adjusted_line = err.line - result.header_lines;
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

void MainWindow::onSettingsClicked() {
    QSettings settings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);

    SettingsDialog dialog(this);
    dialog.setCompilerPath(compilerPath_);
    dialog.setProjectRoot(projectRoot_);
    dialog.setSelectedBoard(QString(activeProfile_.name));
    dialog.setAnalogNoise(analogNoise_);

    if (dialog.exec() == QDialog::Accepted) {
        compilerPath_      = dialog.compilerPath();
        projectRoot_       = dialog.projectRoot();
        int oldSerialCount = activeProfile_.serial_count;
        activeProfile_     = dialog.selectedBoard();
        analogNoise_       = dialog.analogNoise();
        settings.setValue("compiler/path", compilerPath_);
        settings.setValue("compiler/project_root", projectRoot_);
        settings.setValue("board/name", QString(activeProfile_.name));
        settings.setValue("simulation/analog_noise", analogNoise_);
        canvasWidget_->setProfile(activeProfile_);
        boardLabel_->setText(activeProfile_.name);
        if (sketchThread_) sketchThread_->setProfile(activeProfile_);
        if (sketchThread_) sketchThread_->setAnalogNoise(analogNoise_);
        if (activeProfile_.serial_count != oldSerialCount)
            rebuildSerialMonitors();
        statusBar()->showMessage("Settings saved");
    }
}

void MainWindow::onNewSketch() {
    bool ok;
    QString name = QInputDialog::getText(
        this, "New sketch", "Sketch name:",
        QLineEdit::Normal, "my_sketch", &ok
    );
    if (!ok || name.trimmed().isEmpty()) return;

    name = name.trimmed().replace(" ", "_");

    QString sketches_root = QCoreApplication::applicationDirPath() + "/sketches";
    QString sketch_dir    = sketches_root + "/" + name;
    QDir().mkpath(sketch_dir);

    QString default_sketch =
        "#define LED_PIN 13\n\n"
        "void setup() {\n"
        "    Serial.begin(9600);\n"
        "}\n\n"
        "void loop() {\n"
        "}\n";

    QString file_path = sketch_dir + "/" + name + ".cpp";
    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to create: " + file_path);
        return;
    }
    file.write(default_sketch.toUtf8());
    file.close();

    codeEditor_->setPlainText(default_sketch);
    currentSketchPath_ = file_path;
    setWindowTitle("VEMCODE — " + name + ".cpp");
    statusBar()->showMessage("Created: " + file_path);
    addToRecentSketches(file_path);
}

void MainWindow::onRecentSketches() {
    QSettings settings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);
    QStringList recent = settings.value("recent/sketches").toStringList();

    if (recent.isEmpty()) {
        statusBar()->showMessage("No recent sketches");
        return;
    }

    QMenu menu(this);
    for (const QString& path : recent) {
        QAction* action = menu.addAction(QFileInfo(path).fileName());
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            if (!QFile::exists(path)) {
                statusBar()->showMessage("File not found: " + path);
                return;
            }
            currentSketchPath_ = path;
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                codeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
                file.close();
            }
            setWindowTitle("VEMCODE — " + QFileInfo(path).fileName());
            statusBar()->showMessage("Opened: " + path);
            addToRecentSketches(path);
        });
    }
    menu.exec(QCursor::pos());
}

void MainWindow::addToRecentSketches(const QString& path) {
    QSettings settings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);
    QStringList recent = settings.value("recent/sketches").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 5)
        recent.removeLast();
    settings.setValue("recent/sketches", recent);
}

void MainWindow::onSpeedChanged(int value) {
    float display_speed = value / 10.0f;  // slider range 1–25: 1=0.1x, 10=1.0x, 25=2.5x
    sketchThread_->setSpeed(display_speed);
    speedSlider_->setToolTip(QString("Speed: %1x").arg(display_speed, 0, 'f', 1));
    QToolTip::showText(QCursor::pos(), speedSlider_->toolTip(), speedSlider_);
}

void MainWindow::onSerialSend() {
    QString text = serialInput_->text();
    if (text.isEmpty()) return;
    sketchThread_->injectSerial(text + "\n");
    serialMonitor_->appendPlainText("> " + text);
    serialInput_->clear();
}

QStringList MainWindow::runStaticChecks(const QString& source) {
    QStringList warnings;
    std::string src = source.toStdString();

    // Build pin symbol table from #define NAME VALUE and const int NAME = VALUE
    std::map<std::string, int> pin_defs;
    {
        auto scan = [&](const std::regex& re) {
            for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
                 it != std::sregex_iterator(); ++it) {
                std::string name = (*it)[1].str();
                try { pin_defs[name] = std::stoi((*it)[2].str()); } catch (...) {}
            }
        };
        scan(std::regex(R"(#\s*define\s+(\w+)\s+\(?\s*(\d+)\s*\)?)"));
        scan(std::regex(R"(const\s+int\s+(\w+)\s*=\s*(\d+)\s*;)"));
    }

    // 1. Pin out of range for selected board
    for (const auto& [name, pin] : pin_defs) {
        if (!is_pin_name(name)) continue;
        if (pin < 0 || pin >= 80) continue; // not a plausible pin number
        if (pin >= activeProfile_.pin_count) {
            warnings << QString("WARNING: Pin %1 ('%2') is not available on the %3 (max pin %4)")
                .arg(pin)
                .arg(QString::fromStdString(name))
                .arg(activeProfile_.name)
                .arg(activeProfile_.pin_count - 1);
        }
    }

    // 2. analogWrite() on a non-PWM pin
    auto pwm_pins = get_pwm_pins_for(activeProfile_);
    if (!pwm_pins.empty()) {
        std::regex aw_re(R"((?:api->)?analogWrite\s*\(\s*(\w+))");
        std::set<int> warned_pins;
        for (auto it = std::sregex_iterator(src.begin(), src.end(), aw_re);
             it != std::sregex_iterator(); ++it) {
            std::string token = (*it)[1].str();
            int pin = -1;
            try { pin = std::stoi(token); } catch (...) {
                auto def_it = pin_defs.find(token);
                if (def_it != pin_defs.end()) pin = def_it->second;
            }
            if (pin < 0 || warned_pins.count(pin)) continue;
            if (std::find(pwm_pins.begin(), pwm_pins.end(), pin) == pwm_pins.end()) {
                warned_pins.insert(pin);
                warnings << QString("WARNING: Pin %1 does not support PWM on the %2 — analogWrite() will have no effect")
                    .arg(pin).arg(activeProfile_.name);
            }
        }
    }

    // 3. attachInterrupt() with raw interrupt number (Uno/Nano: interrupt 0=pin 2, 1=pin 3)
    // Deduplicate by interrupt number so a matching comment doesn't double the warning.
    {
        std::string board = activeProfile_.name;
        if (board == "Arduino Uno" || board == "Arduino Nano") {
            std::regex ai_re(R"(attachInterrupt\s*\(\s*([01])\s*,)");
            std::set<std::string> warned_nums;
            for (auto it = std::sregex_iterator(src.begin(), src.end(), ai_re);
                 it != std::sregex_iterator(); ++it) {
                std::string n = (*it)[1].str();
                if (warned_nums.count(n)) continue;
                warned_nums.insert(n);
                int correct_pin = (n == "0") ? 2 : 3;
                warnings << QString("WARNING: attachInterrupt(%1, ...) uses an interrupt number, not a pin number — use digitalPinToInterrupt(%2) to attach to pin %2 on the %3")
                    .arg(QString::fromStdString(n)).arg(correct_pin).arg(activeProfile_.name);
            }
        }
    }

    // 4. delay() inside an attachInterrupt callback function
    {
        // Extract function body by brace-counting from the opening {
        auto func_body = [&](const std::string& func_name) -> std::string {
            std::regex func_re("\\bvoid\\s+" + func_name + R"(\s*\(\s*\)\s*\{)");
            std::smatch m;
            std::string s = src;
            if (!std::regex_search(s, m, func_re)) return "";
            size_t pos = static_cast<size_t>(m.position()) + m.length() - 1;
            int depth = 1;
            size_t start = pos + 1;
            while (pos < src.size() - 1 && depth > 0) {
                ++pos;
                if (src[pos] == '{') ++depth;
                else if (src[pos] == '}') --depth;
            }
            return src.substr(start, pos - start);
        };

        std::regex ai_re(R"(attachInterrupt\s*\([^,]+,\s*(\w+)\s*,)");
        std::set<std::string> warned_funcs;
        for (auto it = std::sregex_iterator(src.begin(), src.end(), ai_re);
             it != std::sregex_iterator(); ++it) {
            std::string fn = (*it)[1].str();
            if (warned_funcs.count(fn)) continue;
            std::string body = func_body(fn);
            if (!body.empty() && body.find("delay(") != std::string::npos) {
                warned_funcs.insert(fn);
                warnings << QString("WARNING: delay() inside '%1' (used as an interrupt handler) will hang on real Arduino — interrupts are disabled during ISR execution")
                    .arg(QString::fromStdString(fn));
            }
        }
    }

    // 5. Pin defined as an expression (CircuitDetector cannot evaluate it)
    {
        std::regex def_re(R"(#\s*define\s+(\w+)\s+(.+))");
        for (auto it = std::sregex_iterator(src.begin(), src.end(), def_re);
             it != std::sregex_iterator(); ++it) {
            std::string name  = (*it)[1].str();
            std::string value = (*it)[2].str();
            // Strip trailing comment and whitespace
            auto comment = value.find("//");
            if (comment != std::string::npos) value = value.substr(0, comment);
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
                value.pop_back();
            // Only flag if the value contains arithmetic operators
            bool has_op = value.find('+') != std::string::npos ||
                          value.find('*') != std::string::npos ||
                          value.find('/') != std::string::npos ||
                          value.find("<<") != std::string::npos;
            // '-' only counts as an operator if not at the start (negative literal)
            if (!has_op && value.size() > 1 && value.find('-') != std::string::npos)
                has_op = (value.find('-') != 0 &&
                          !(value.find('-') == 1 && value[0] == '('));
            if (!has_op) continue;
            if (!is_pin_name(name)) continue;
            warnings << QString("WARNING: Pin '%1' is defined as an expression — the simulator could not evaluate it and the component may not appear on the canvas; use a plain number instead")
                .arg(QString::fromStdString(name));
        }
    }

    return warnings;
}