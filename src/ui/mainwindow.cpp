#include "src/ui/mainwindow.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/componentitem.h"
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
#include <QApplication>
#include <QTextEdit>
#include <QInputDialog>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QToolTip>
#include <QIcon>
#include <QWheelEvent>
#include <QMessageBox>

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

// Font size intentionally left out of the editor's styling -- it's set via
// setFont() below so QPlainTextEdit::zoomIn()/zoomOut() (which adjust the
// widget's QFont directly) aren't fought by a QSS-cascaded font-size.
// Was "font-size: 13px" in the old stylesheet -- points render larger than the
// same numeric pixel value, so this is 13px converted to points (13 * 0.75).
static const int DEFAULT_EDITOR_FONT_SIZE = 10;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    updateWindowTitle();
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
    defaultSketchLocation_ = settings.value("sketches/default_location",
        QCoreApplication::applicationDirPath() + "/sketches").toString();

    QString boardName = settings.value("board/name", "Arduino Uno").toString();
    if      (boardName == "Arduino Nano")       activeProfile_ = BOARD_NANO;
    else if (boardName == "Arduino Mega 2560")  activeProfile_ = BOARD_MEGA;
    else if (boardName == "Arduino Due")        activeProfile_ = BOARD_DUE;
    else if (boardName == "Teensy 4.1")         activeProfile_ = BOARD_TEENSY;
    else                                        activeProfile_ = BOARD_UNO;

    if (compilerPath_.isEmpty() || projectRoot_.isEmpty()) {
        SettingsDialog dialog(this);
        QString detectedPath, detectedRoot;
        if (SettingsDialog::autoDetectCompiler(detectedPath, detectedRoot)) {
            dialog.setCompilerPath(detectedPath);
            dialog.setProjectRoot(detectedRoot);
        }
        if (dialog.exec() == QDialog::Accepted) {
            compilerPath_ = dialog.compilerPath();
            projectRoot_  = dialog.projectRoot();
            settings.setValue("compiler/path", compilerPath_);
            settings.setValue("compiler/project_root", projectRoot_);
            applyKeybinds(dialog.keybinds());
        }
    }

    setupToolbar(central, layout);
    boardLabel_->setText(activeProfile_.name);

    setupMainArea(central, layout);  // variableWatch_ must exist before the connect below
    canvasWidget_->setProfile(activeProfile_);
    darkTheme_ = settings.value("canvas/dark_theme", true).toBool();
    setAppTheme(darkTheme_);

    sketchThread_ = new SketchThread(this);
    sketchThread_->setProfile(activeProfile_);
    analogNoise_ = settings.value("simulation/analog_noise", false).toBool();
    sketchThread_->setAnalogNoise(analogNoise_);
    autoCompileOnSave_ = settings.value("editor/auto_compile_on_save", false).toBool();
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
    connect(sketchThread_, &SketchThread::watchdogReset,
            this, [this]() {
                serialMonitor_->appendPlainText("WARNING: Watchdog reset — wdt_reset() was not called in time\n");
                onStopClicked();
                onRunClicked();
            });
    connect(sketchThread_, &SketchThread::sleepChanged,
            this, [this](bool sleeping) {
                statusBar()->showMessage(sleeping ? "Sleeping…" : "Running");
            });
    connect(sketchThread_, &SketchThread::variableChanged,
            variableWatch_, &VariableWatch::onVariableChanged);
    connect(sketchThread_, &SketchThread::lcdPrint,
            canvasWidget_, &CanvasWidget::updateLcdText);
    connect(devicesPanel_, &DevicesPanel::deviceChanged,
            this, [this](int address, std::vector<uint8_t> bytes) {
                sketchThread_->injectWireDevice(address, bytes);
            });
    connect(spiPanel_, &SpiPanel::bytesChanged,
            this, [this](std::vector<uint8_t> bytes) {
                sketchThread_->injectSpiBytes(bytes);
            });
    connect(variableWatch_, &VariableWatch::watchListChanged,
            this, [this](std::vector<std::pair<QString, QString>> vars) {
                sketchThread_->setWatchList(vars);
            });

    QShortcut* save_shortcut = new QShortcut(loadKeybind(settings, "save", QKeySequence::Save), this);
    connect(save_shortcut, &QShortcut::activated, this, &MainWindow::onSaveClicked);
    keybindShortcuts_["save"] = save_shortcut;
    QShortcut* save_as_shortcut = new QShortcut(loadKeybind(settings, "save_as", QKeySequence::SaveAs), this);
    connect(save_as_shortcut, &QShortcut::activated, this, &MainWindow::onSaveAsClicked);
    keybindShortcuts_["save_as"] = save_as_shortcut;

    // Mirrors runButton_'s own disabled-while-running guard -- a shortcut has
    // no disabled state of its own to stop it firing mid-run.
    QShortcut* run_shortcut = new QShortcut(loadKeybind(settings, "run", QKeySequence(Qt::CTRL | Qt::Key_R)), this);
    connect(run_shortcut, &QShortcut::activated, this, [this]() {
        if (runButton_->isEnabled()) onRunClicked();
    });
    keybindShortcuts_["run"] = run_shortcut;

    // Explicit key sequences instead of QKeySequence::ZoomIn/ZoomOut -- the platform
    // standard key for ZoomIn resolves to the same sequence as literal "Ctrl+=" on this
    // setup, and two QShortcuts sharing one sequence make Qt treat it as ambiguous
    // (neither fires). Bind both the unshifted "=" and shifted "+" explicitly instead;
    // only the unshifted one is user-remappable, the shifted one is a fixed convenience
    // alias so layouts where "+" is easier to reach than bare "=" still work.
    QShortcut* zoom_in_shortcut = new QShortcut(loadKeybind(settings, "editor_zoom_in", QKeySequence(Qt::CTRL | Qt::Key_Equal)), this);
    connect(zoom_in_shortcut, &QShortcut::activated, this, [this]() { adjustEditorZoom(1); });
    keybindShortcuts_["editor_zoom_in"] = zoom_in_shortcut;
    QShortcut* zoom_in_shortcut_shift = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Equal), this);
    connect(zoom_in_shortcut_shift, &QShortcut::activated, this, [this]() { adjustEditorZoom(1); });
    QShortcut* zoom_out_shortcut = new QShortcut(loadKeybind(settings, "editor_zoom_out", QKeySequence(Qt::CTRL | Qt::Key_Minus)), this);
    connect(zoom_out_shortcut, &QShortcut::activated, this, [this]() { adjustEditorZoom(-1); });
    keybindShortcuts_["editor_zoom_out"] = zoom_out_shortcut;
    QShortcut* zoom_reset_shortcut = new QShortcut(loadKeybind(settings, "editor_zoom_reset", QKeySequence(Qt::CTRL | Qt::Key_0)), this);
    connect(zoom_reset_shortcut, &QShortcut::activated, this, &MainWindow::resetEditorZoom);
    keybindShortcuts_["editor_zoom_reset"] = zoom_reset_shortcut;

    // Same unshifted/shifted pairing as the editor zoom shortcuts above, on Alt
    // instead of Ctrl so it doesn't collide with editor font zoom.
    QShortcut* canvas_zoom_in_shortcut = new QShortcut(loadKeybind(settings, "canvas_zoom_in", QKeySequence(Qt::ALT | Qt::Key_Equal)), this);
    connect(canvas_zoom_in_shortcut, &QShortcut::activated, this, [this]() { canvasWidget_->zoomIn(); });
    keybindShortcuts_["canvas_zoom_in"] = canvas_zoom_in_shortcut;
    QShortcut* canvas_zoom_in_shortcut_shift = new QShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal), this);
    connect(canvas_zoom_in_shortcut_shift, &QShortcut::activated, this, [this]() { canvasWidget_->zoomIn(); });
    QShortcut* canvas_zoom_out_shortcut = new QShortcut(loadKeybind(settings, "canvas_zoom_out", QKeySequence(Qt::ALT | Qt::Key_Minus)), this);
    connect(canvas_zoom_out_shortcut, &QShortcut::activated, this, [this]() { canvasWidget_->zoomOut(); });
    keybindShortcuts_["canvas_zoom_out"] = canvas_zoom_out_shortcut;

    connect(codeEditor_->document(), &QTextDocument::modificationChanged,
            this, [this](bool) { updateWindowTitle(); });

    QShortcut* find_shortcut = new QShortcut(loadKeybind(settings, "find", QKeySequence::Find), this);
    connect(find_shortcut, &QShortcut::activated, this, &MainWindow::showFindBar);
    keybindShortcuts_["find"] = find_shortcut;

    // code_completion, duplicate_line, and comment_toggle have no QShortcut of
    // their own -- they're raw key comparisons in eventFilter (scoped to
    // codeEditor_ only), so just seed keybindSeq_ with their current sequence.
    loadKeybind(settings, "code_completion", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space));
    loadKeybind(settings, "duplicate_line", QKeySequence(Qt::CTRL | Qt::Key_D));
    loadKeybind(settings, "comment_toggle", QKeySequence(Qt::CTRL | Qt::Key_Slash));

    // Autosave every 30s, but only for a sketch with a real (non-scratch) path
    // and only if there are actually unsaved changes to write
    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setInterval(30000);
    connect(autosaveTimer_, &QTimer::timeout, this, [this]() {
        bool has_real_path = !currentSketchPath_.isEmpty()
            && !currentSketchPath_.startsWith(QDir::tempPath());
        if (!has_real_path || !codeEditor_->document()->isModified()) return;

        QFile file(currentSketchPath_ + ".autosave");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(codeEditor_->toPlainText().toUtf8());
            file.close();
        }
    });
    autosaveTimer_->start();

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    if (sketchThread_)
        sketchThread_->stopSketch();
}

// A clean exit means nothing to recover -- drop any pending autosave
void MainWindow::closeEvent(QCloseEvent* event) {
    bool has_real_path = !currentSketchPath_.isEmpty()
        && !currentSketchPath_.startsWith(QDir::tempPath());
    if (has_real_path)
        QFile::remove(currentSketchPath_ + ".autosave");
    QMainWindow::closeEvent(event);
}

// Toolbar -- Run, Stop, Open, Save, board label
void MainWindow::setupToolbar(QWidget* parent, QVBoxLayout* layout) {
    QWidget*     toolbar      = new QWidget(parent);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 6);
    toolbarLayout->setSpacing(6);
    toolbar->setObjectName("toolbar");
    toolbar->setFixedHeight(42);

    QLabel* logo = new QLabel(toolbar);
    logo->setPixmap(QIcon(":/logo.svg").pixmap(36, 36));
    toolbarLayout->addWidget(logo);
    toolbarLayout->addSpacing(6);

    QLabel* title = new QLabel("VEMCODE", toolbar);
    title->setObjectName("appTitle");
    toolbarLayout->addWidget(title);
    toolbarLayout->addSpacing(8);

    runButton_ = new QPushButton("Run", toolbar);
    runButton_->setFixedSize(64, 26);
    runButton_->setObjectName("btnRun");
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    toolbarLayout->addWidget(runButton_);

    stopButton_ = new QPushButton("Stop", toolbar);
    stopButton_->setFixedSize(64, 26);
    stopButton_->setEnabled(false);
    stopButton_->setObjectName("btnStop");
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    toolbarLayout->addWidget(stopButton_);

    QLabel* speedLabel = new QLabel("Speed:", toolbar);
    speedLabel->setProperty("role", "muted-label");
    toolbarLayout->addWidget(speedLabel);

    speedSlider_ = new QSlider(Qt::Horizontal, toolbar);
    speedSlider_->setRange(1, 25);
    speedSlider_->setValue(10);
    speedSlider_->setFixedWidth(80);
    speedSlider_->setToolTip("Simulation speed (5 = normal)");
    connect(speedSlider_, &QSlider::valueChanged, this, &MainWindow::onSpeedChanged);
    toolbarLayout->addWidget(speedSlider_);

    QPushButton* newsketchButton = new QPushButton("New Sketch", toolbar);
    newsketchButton->setFixedHeight(26);
    newsketchButton->setProperty("role", "outline");
    connect(newsketchButton, &QPushButton::clicked, this, &MainWindow::onNewSketch);
    toolbarLayout->addWidget(newsketchButton);

    QPushButton* openButton = new QPushButton("Open Sketch", toolbar);
    openButton->setFixedHeight(26);
    openButton->setProperty("role", "outline");
    connect(openButton, &QPushButton::clicked, this, &MainWindow::onOpenClicked);
    toolbarLayout->addWidget(openButton);

    QPushButton* recentButton = new QPushButton("Recent", toolbar);
    recentButton->setFixedHeight(26);
    recentButton->setProperty("role", "outline");
    connect(recentButton, &QPushButton::clicked, this, &MainWindow::onRecentSketches);
    toolbarLayout->addWidget(recentButton);

    QPushButton* saveButton = new QPushButton("Save", toolbar);
    saveButton->setFixedHeight(26);
    saveButton->setProperty("role", "outline");
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    toolbarLayout->addWidget(saveButton);

    QPushButton* saveAsButton = new QPushButton("Save As", toolbar);
    saveAsButton->setFixedHeight(26);
    saveAsButton->setProperty("role", "outline");
    connect(saveAsButton, &QPushButton::clicked, this, &MainWindow::onSaveAsClicked);
    toolbarLayout->addWidget(saveAsButton);

    QPushButton* settingsButton = new QPushButton("Settings", toolbar);
    settingsButton->setFixedHeight(26);
    settingsButton->setProperty("role", "outline");
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    toolbarLayout->addWidget(settingsButton);

    toolbarLayout->addStretch();

    boardLabel_ = new QLabel("", toolbar);
    boardLabel_->setObjectName("boardLabel");
    toolbarLayout->addWidget(boardLabel_);

    layout->addWidget(toolbar);
}

// Main area -- horizontal splitter: editor | (canvas + debug)
void MainWindow::setupMainArea(QWidget* parent, QVBoxLayout* layout) {
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, parent);
    mainSplitter->setHandleWidth(1);
    mainSplitter->addWidget(buildEditorPanel());

    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    rightSplitter->setHandleWidth(1);
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
    header->setProperty("role", "panel-header");
    layout->addWidget(header);

    // Find & Replace bar -- hidden until Ctrl+F/Ctrl+H
    findBar_ = new QWidget();
    findBar_->setObjectName("findBar");
    findBar_->setProperty("role", "panel-header"); // same bg/border look as panel headers
    QVBoxLayout* findBarLayout = new QVBoxLayout(findBar_);
    findBarLayout->setContentsMargins(8, 6, 8, 6);
    findBarLayout->setSpacing(4);

    QHBoxLayout* findRowLayout = new QHBoxLayout();
    findRowLayout->setSpacing(6);
    findInput_ = new QLineEdit(findBar_);
    findInput_->setPlaceholderText("Find");
    findRowLayout->addWidget(findInput_);

    findStatusLabel_ = new QLabel(findBar_);
    findStatusLabel_->setProperty("role", "muted-label");
    findStatusLabel_->setFixedWidth(60);
    findRowLayout->addWidget(findStatusLabel_);

    QPushButton* findPrevButton = new QPushButton("Prev", findBar_);
    findPrevButton->setFixedHeight(24);
    findPrevButton->setProperty("role", "outline");
    connect(findPrevButton, &QPushButton::clicked, this, &MainWindow::onFindPrev);
    findRowLayout->addWidget(findPrevButton);

    QPushButton* findNextButton = new QPushButton("Next", findBar_);
    findNextButton->setFixedHeight(24);
    findNextButton->setProperty("role", "outline");
    connect(findNextButton, &QPushButton::clicked, this, &MainWindow::onFindNext);
    findRowLayout->addWidget(findNextButton);

    QPushButton* findCloseButton = new QPushButton("✕", findBar_);
    findCloseButton->setFixedSize(24, 24);
    findCloseButton->setProperty("role", "outline");
    connect(findCloseButton, &QPushButton::clicked, this, &MainWindow::hideFindBar);
    findRowLayout->addWidget(findCloseButton);

    findBarLayout->addLayout(findRowLayout);

    replaceRow_ = new QWidget(findBar_);
    QHBoxLayout* replaceRowLayout = new QHBoxLayout(replaceRow_);
    replaceRowLayout->setContentsMargins(0, 0, 0, 0);
    replaceRowLayout->setSpacing(6);
    replaceInput_ = new QLineEdit(replaceRow_);
    replaceInput_->setPlaceholderText("Replace");
    replaceRowLayout->addWidget(replaceInput_);

    QPushButton* replaceButton = new QPushButton("Replace", replaceRow_);
    replaceButton->setFixedHeight(24);
    replaceButton->setProperty("role", "outline");
    connect(replaceButton, &QPushButton::clicked, this, &MainWindow::onReplaceClicked);
    replaceRowLayout->addWidget(replaceButton);

    QPushButton* replaceAllButton = new QPushButton("Replace All", replaceRow_);
    replaceAllButton->setFixedHeight(24);
    replaceAllButton->setProperty("role", "outline");
    connect(replaceAllButton, &QPushButton::clicked, this, &MainWindow::onReplaceAllClicked);
    replaceRowLayout->addWidget(replaceAllButton);

    findBarLayout->addWidget(replaceRow_);

    connect(findInput_, &QLineEdit::textChanged, this, [this](const QString&) { runFindSearch(); });
    findInput_->installEventFilter(this);
    replaceInput_->installEventFilter(this);

    findBar_->hide();
    layout->addWidget(findBar_);

    codeEditor_ = new EditorWithLines();
    highlighter_ = new CodeHighlighter(codeEditor_->document());
    codeEditor_->setObjectName("codeEditor");
    codeEditor_->setLineWrapMode(QPlainTextEdit::NoWrap);

    QFont editorFont("Courier New", DEFAULT_EDITOR_FONT_SIZE);
    editorFont.setStyleHint(QFont::Monospace);
    codeEditor_->setFont(editorFont);

    // Tab width = 4 spaces
    QFontMetrics metrics(codeEditor_->font());
    codeEditor_->setTabStopDistance(4 * metrics.horizontalAdvance(' '));
    codeEditor_->installEventFilter(this);
    connect(codeEditor_, &QPlainTextEdit::cursorPositionChanged,
            this, &MainWindow::updateBracketMatch);

    // Auto Complete
    completer_ = new QCompleter(this);
    completer_->setWidget(codeEditor_);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    connect(completer_, QOverload<const QString&>::of(&QCompleter::activated),
            this, &MainWindow::insertCompletion);
    completer_->popup()->installEventFilter(this);

    // Auto-popup: after typing >= 3 characters of a word, wait 5s idle then
    // show the same completion popup Ctrl+Shift+Space triggers manually.
    idleCompletionTimer_ = new QTimer(this);
    idleCompletionTimer_->setSingleShot(true);
    idleCompletionTimer_->setInterval(1500);
    connect(idleCompletionTimer_, &QTimer::timeout, this, [this]() {
        if (!codeEditor_->hasFocus() || completer_->popup()->isVisible()) return;
        QTextCursor tc = codeEditor_->textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        if (tc.selectedText().length() >= 3)
            showCompletionPopup();
    });
    connect(codeEditor_, &QPlainTextEdit::textChanged, this, [this]() {
        if (!codeEditor_->hasFocus() || completer_->popup()->isVisible()) {
            idleCompletionTimer_->stop();
            return;
        }
        QTextCursor tc = codeEditor_->textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        if (tc.selectedText().length() >= 3)
            idleCompletionTimer_->start();
        else
            idleCompletionTimer_->stop();
    });

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

    QWidget*     header       = new QWidget();
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    header->setFixedHeight(24);
    header->setProperty("role", "panel-header");
    headerLayout->setContentsMargins(8, 0, 6, 0);
    headerLayout->setSpacing(0);

    QLabel* headerLabel = new QLabel("CIRCUIT CANVAS");
    headerLabel->setStyleSheet("border: none; background: transparent;");
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch();

    layoutButton_ = new QPushButton("Layout", header);
    layoutButton_->setCheckable(true);
    layoutButton_->setFixedSize(56, 18);
    layoutButton_->setProperty("role", "toggle");
    layoutButton_->setToolTip("Toggle layout mode to drag components to new positions");
    connect(layoutButton_, &QPushButton::toggled, this, &MainWindow::onLayoutToggled);
    headerLayout->addWidget(layoutButton_);

    headerLayout->addSpacing(4);

    QPushButton* resetLayoutButton = new QPushButton("Reset", header);
    resetLayoutButton->setFixedSize(48, 18);
    resetLayoutButton->setProperty("role", "toggle");
    resetLayoutButton->setToolTip("Reset components to their auto-arranged positions");
    connect(resetLayoutButton, &QPushButton::clicked, this, [this]() {
        canvasWidget_->resetLayout();
    });
    headerLayout->addWidget(resetLayoutButton);

    layout->addWidget(header);

    canvasWidget_ = new CanvasWidget();
    layout->addWidget(canvasWidget_);

    connect(canvasWidget_, &CanvasWidget::inputChanged,
            this, &MainWindow::onComponentInput);

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
    serialMonitor_->setProperty("role", "serial");

    int serialCount = activeProfile_.serial_count;

    if (serialCount >= 2) {
        QSplitter* serialSplitter = new QSplitter(Qt::Horizontal, serialPanel);
        serialSplitter->setHandleWidth(1);

        auto makePane = [&](QPlainTextEdit* mon, const QString& label, bool bordered) {
            QWidget*     pane   = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(pane);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            QLabel* lbl = new QLabel(label, pane);
            lbl->setFixedHeight(18);
            lbl->setProperty("role", bordered ? "port-label-bordered" : "port-label");
            layout->addWidget(lbl);
            layout->addWidget(mon);
            return pane;
        };

        serialSplitter->addWidget(makePane(serialMonitor_, "  Serial", false));

        serialMonitor1_ = new QPlainTextEdit();
        serialMonitor1_->setReadOnly(true);
        serialMonitor1_->setMaximumBlockCount(2000);
        serialMonitor1_->setProperty("role", "serial");
        serialSplitter->addWidget(makePane(serialMonitor1_, "  Serial1", true));

        if (serialCount >= 3) {
            serialMonitor2_ = new QPlainTextEdit();
            serialMonitor2_->setReadOnly(true);
            serialMonitor2_->setMaximumBlockCount(2000);
            serialMonitor2_->setProperty("role", "serial");
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
    inputRow->setProperty("role", "input-row");
    inputRow->setFixedHeight(36);

    serialInput_ = new QLineEdit();
    serialInput_->setPlaceholderText("Send serial input...");
    inputLayout->addWidget(serialInput_);

    QPushButton* sendButton = new QPushButton("Send", inputRow);
    sendButton->setFixedWidth(50);
    sendButton->setProperty("role", "outline");
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

    debugTabs_->addTab(buildSerialPanel(), "Serial monitor");
    signalTimeline_ = new SignalTimeline();
    debugTabs_->addTab(signalTimeline_, "Signal timeline");
    variableWatch_ = new VariableWatch();
    debugTabs_->addTab(variableWatch_, "Variable watch");
    devicesPanel_ = new DevicesPanel();
    debugTabs_->addTab(devicesPanel_, "I2C");
    spiPanel_ = new SpiPanel();
    debugTabs_->addTab(spiPanel_, "SPI");

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

void MainWindow::onComponentInput(int pin, int eventType, QVariant value) {
    switch (static_cast<ComponentEventType>(eventType)) {
        case ComponentEventType::DigitalPress:
            sketchThread_->injectPin(pin, value.toInt());
            break;
        case ComponentEventType::BouncePress:
            sketchThread_->injectButtonBounce(pin, value.toInt());
            break;
        case ComponentEventType::AnalogValue:
            sketchThread_->injectAnalog(pin, value.toInt());
            break;
        case ComponentEventType::PulseUs:
            sketchThread_->injectPulseDuration(pin, value.toULongLong());
            break;
        case ComponentEventType::ColorRGB: {
            QVariantList c = value.toList();
            sketchThread_->injectColor(
                pin, c.value(3).toInt(), c.value(4).toInt(),
                c.value(0).toInt(), c.value(1).toInt(), c.value(2).toInt());
            break;
        }
    }
}

void MainWindow::insertCompletion(const QString& completion) {
    QTextCursor tc = codeEditor_->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    tc.insertText(completion);
    codeEditor_->setTextCursor(tc);
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
    // If no file is open, save editor content to a temp file and run that.
    // Uses its own subfolder rather than tempPath() directly -- the compiler scans
    // the sketch's folder for extra .cpp files to link in (multi-file sketch
    // support), and raw /tmp can contain unrelated .cpp files from other processes.
    if (currentSketchPath_.isEmpty()) {
        QString temp_dir = QDir::tempPath() + "/vemcode_unsaved_sketch";
        QDir().mkpath(temp_dir);
        QString temp_path = temp_dir + "/vb_sketch.cpp";
        QFile temp_file(temp_path);
        if (temp_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            temp_file.write(codeEditor_->toPlainText().toUtf8());
            temp_file.close();
        }
        currentSketchPath_ = temp_path;
        windowTitleBase_ = "VEMCODE — unsaved sketch";
        updateWindowTitle();
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

    showCompileErrors(result); // paints warning lines only -- no errors on this path
    signalTimeline_->clear();
    variableWatch_->clearValues();
    simTimer_.start();
    statusBar()->showMessage("Running: " + currentSketchPath_);
    stopButton_->setEnabled(true);

    // Reset state and inject component values before starting the sketch --
    // otherwise it can read pins before injection, or see stale prior-run state.
    sketchThread_->stopSketch();
    sketchThread_->resetRuntimeState();

    detector_.detect(codeEditor_->toPlainText().toStdString());
    canvasWidget_->refresh(detector_.components());
    devicesPanel_->pushAll(); // re-inject the Devices table into the freshly reset runtime
    spiPanel_->pushAll(); // re-inject the SPI response sequence into the freshly reset runtime

    // Show detected components on the serial monitor
    serialMonitor_->appendPlainText(QString::fromStdString("=== Components detected ===\n"));
    for (const auto& comp : detector_.components())
        serialMonitor_->appendPlainText(QString::fromStdString(comp.to_string()));
    serialMonitor_->appendPlainText(QString::fromStdString("===========================\n"));

    // Same-pin-claimed-by-two-components warnings
    for (const auto& w : detector_.warnings())
        serialMonitor_->appendPlainText(QString::fromStdString(w) + "\n");

    // No-components-detected hints
    const auto& comps = detector_.components();
    bool has_non_serial = std::any_of(comps.begin(), comps.end(),
        [](const DetectedComponent& c) { return c.type_name != "Serial"; });
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

    sketchThread_->startSketch(QString::fromStdString(result.dll_path));
}

void MainWindow::onStopClicked() {
    sketchThread_->stopSketch();
    statusBar()->showMessage("Stopped");
    runButton_->setEnabled(true);
    stopButton_->setEnabled(false);
}

void MainWindow::onLayoutToggled(bool on) {
    canvasWidget_->setLayoutMode(on);
    statusBar()->showMessage(on ? "Layout mode: drag components to reposition" : "Ready");
}

void MainWindow::onOpenClicked() {
    QString sketches_root = defaultSketchLocation_;
    QString path = QFileDialog::getOpenFileName(
        this, "Open sketch", sketches_root, "C++ files (*.cpp *.ino)"
    );
    if (path.isEmpty()) return;

    currentSketchPath_ = path;
    canvasWidget_->loadLayout(path);

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        codeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
        codeEditor_->document()->setModified(false);
        file.close();
    }

    windowTitleBase_ = "VEMCODE — " + QFileInfo(path).fileName();
    updateWindowTitle();
    checkForAutosaveRecovery(path);
    statusBar()->showMessage("Opened: " + path);
    addToRecentSketches(path);
}

void MainWindow::onSaveClicked() {
    // Silently overwrite the already-open sketch -- but a path under the OS temp
    // dir (see onRunClicked's unsaved-scratch-file fallback) doesn't count as
    // "already open", so that case still prompts for a real name below.
    bool has_real_path = !currentSketchPath_.isEmpty()
        && !currentSketchPath_.startsWith(QDir::tempPath());
    if (!has_real_path) {
        promptAndSaveAsNewSketch();
        return;
    }

    QFile file(currentSketchPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to save: " + currentSketchPath_);
        return;
    }
    file.write(codeEditor_->toPlainText().toUtf8());
    file.close();
    codeEditor_->document()->setModified(false);
    QFile::remove(currentSketchPath_ + ".autosave");
    canvasWidget_->saveLayout(currentSketchPath_);
    statusBar()->showMessage("Saved: " + currentSketchPath_);
    addToRecentSketches(currentSketchPath_);

    if (autoCompileOnSave_ && runButton_->isEnabled()) onRunClicked();
}

// Always prompts for a name and creates a new sketch, even if one is already open
void MainWindow::onSaveAsClicked() {
    promptAndSaveAsNewSketch();
}

void MainWindow::promptAndSaveAsNewSketch() {
    bool ok;
    QString name = QInputDialog::getText(
        this, "Save sketch", "Sketch name:",
        QLineEdit::Normal, "my_sketch", &ok
    );
    if (!ok || name.trimmed().isEmpty()) return;

    name = name.trimmed().replace(" ", "_");

    QString sketches_root = defaultSketchLocation_;
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
    canvasWidget_->saveLayout(file_path);
    codeEditor_->document()->setModified(false);
    QFile::remove(file_path + ".autosave");
    windowTitleBase_ = "VEMCODE — " + name + ".cpp";
    updateWindowTitle();
    statusBar()->showMessage("Saved: " + file_path);
    addToRecentSketches(file_path);

    if (autoCompileOnSave_ && runButton_->isEnabled()) onRunClicked();
}

// If sketchPath has a newer .autosave file sitting next to it (crash recovery),
// offer to load that instead of what was just read from disk
void MainWindow::checkForAutosaveRecovery(const QString& sketchPath) {
    QFileInfo autosaveInfo(sketchPath + ".autosave");
    if (!autosaveInfo.exists()) return;

    QFileInfo sketchInfo(sketchPath);
    if (autosaveInfo.lastModified() <= sketchInfo.lastModified()) return;

    auto reply = QMessageBox::question(this, "Restore autosave?",
        "An autosave from " + autosaveInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss") +
        " is newer than the saved file. Restore it?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QFile file(autosaveInfo.filePath());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            codeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
            file.close();
            codeEditor_->document()->setModified(true);
        }
    } else {
        QFile::remove(autosaveInfo.filePath());
    }
}

// Window title reflects windowTitleBase_ plus a "*" while the editor has unsaved changes
void MainWindow::updateWindowTitle() {
    bool modified = codeEditor_ && codeEditor_->document()->isModified();
    setWindowTitle(windowTitleBase_ + (modified ? " *" : ""));
}

void MainWindow::adjustEditorZoom(int steps) {
    editorZoomLevel_ = qBound(-8, editorZoomLevel_ + steps, 20);
    QFont f = codeEditor_->font();
    f.setPointSize(qMax(6, DEFAULT_EDITOR_FONT_SIZE + editorZoomLevel_));
    codeEditor_->setFont(f);
}

void MainWindow::resetEditorZoom() {
    editorZoomLevel_ = 0;
    QFont f = codeEditor_->font();
    f.setPointSize(DEFAULT_EDITOR_FONT_SIZE);
    codeEditor_->setFont(f);
}

// Toggles "// " on every line touched by the selection (or just the current line
// with no selection). Uncomments if every non-blank line in range already starts
// with "//", otherwise comments every non-blank line.
void MainWindow::toggleCommentSelection() {
    QTextCursor cursor = codeEditor_->textCursor();
    QTextBlock startBlock = codeEditor_->document()->findBlock(cursor.selectionStart());
    QTextBlock endBlock   = codeEditor_->document()->findBlock(cursor.selectionEnd());
    if (endBlock != startBlock && cursor.selectionEnd() == endBlock.position())
        endBlock = endBlock.previous();

    bool allCommented = true;
    for (QTextBlock b = startBlock; b.isValid(); b = b.next()) {
        QString trimmed = b.text().trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith("//")) allCommented = false;
        if (b == endBlock) break;
    }

    cursor.beginEditBlock();
    for (QTextBlock b = startBlock; b.isValid(); b = b.next()) {
        QString text = b.text();
        int firstNonSpace = 0;
        while (firstNonSpace < text.length() && text[firstNonSpace] == ' ') firstNonSpace++;

        QTextCursor lineCursor(b);
        if (allCommented) {
            if (text.mid(firstNonSpace, 2) == "//") {
                int len = (text.mid(firstNonSpace, 3) == "// ") ? 3 : 2;
                lineCursor.setPosition(b.position() + firstNonSpace);
                lineCursor.setPosition(b.position() + firstNonSpace + len, QTextCursor::KeepAnchor);
                lineCursor.removeSelectedText();
            }
        } else if (!text.trimmed().isEmpty()) {
            lineCursor.setPosition(b.position() + firstNonSpace);
            lineCursor.insertText("// ");
        }
        if (b == endBlock) break;
    }
    cursor.endEditBlock();
}

void MainWindow::refreshExtraSelections() {
    codeEditor_->setExtraSelections(compileSelections_ + bracketSelections_ + findSelections_);
}

// Highlights the bracket pair adjacent to the cursor, if any. Checks the character
// just after the cursor first, then just before, matching common editor convention.
void MainWindow::updateBracketMatch() {
    bracketSelections_.clear();

    static const QString kOpeners = "({[";
    static const QString kClosers = ")}]";

    QTextDocument* doc = codeEditor_->document();
    int pos = codeEditor_->textCursor().position();

    int bracketPos = -1;
    QChar c;
    if (!doc->characterAt(pos).isNull() &&
        (kOpeners.contains(doc->characterAt(pos)) || kClosers.contains(doc->characterAt(pos)))) {
        bracketPos = pos;
        c = doc->characterAt(pos);
    } else if (pos > 0 &&
               (kOpeners.contains(doc->characterAt(pos - 1)) || kClosers.contains(doc->characterAt(pos - 1)))) {
        bracketPos = pos - 1;
        c = doc->characterAt(pos - 1);
    }

    if (bracketPos >= 0) {
        bool isOpen = kOpeners.contains(c);
        QChar match = isOpen ? kClosers[kOpeners.indexOf(c)] : kOpeners[kClosers.indexOf(c)];
        int direction = isOpen ? 1 : -1;

        int depth = 0;
        int i = bracketPos;
        int matchPos = -1;
        while (true) {
            QChar ch = doc->characterAt(i);
            if (ch.isNull()) break;
            if (ch == c) depth++;
            else if (ch == match) {
                depth--;
                if (depth == 0) { matchPos = i; break; }
            }
            i += direction;
        }

        if (matchPos >= 0) {
            for (int p : {bracketPos, matchPos}) {
                QTextEdit::ExtraSelection sel;
                sel.format.setBackground(highlightColors_.bracket_match_bg);
                sel.cursor = QTextCursor(doc);
                sel.cursor.setPosition(p);
                sel.cursor.setPosition(p + 1, QTextCursor::KeepAnchor);
                bracketSelections_.append(sel);
            }
        }
    }

    refreshExtraSelections();
}

void MainWindow::showFindBar() {
    findBar_->show();

    QString selected = codeEditor_->textCursor().selectedText();
    if (!selected.isEmpty() && !selected.contains(QChar::ParagraphSeparator))
        findInput_->setText(selected);

    findInput_->setFocus();
    findInput_->selectAll();
    runFindSearch();
}

void MainWindow::hideFindBar() {
    findBar_->hide();
    findSelections_.clear();
    findMatches_.clear();
    currentMatchIndex_ = -1;
    refreshExtraSelections();
    codeEditor_->setFocus();
}

// Re-scans the whole document for findInput_'s text, highlights every match, and
// jumps to whichever match is nearest the current cursor position.
void MainWindow::runFindSearch() {
    QString needle = findInput_->text();
    findMatches_.clear();
    findSelections_.clear();

    if (!needle.isEmpty()) {
        QTextDocument* doc = codeEditor_->document();
        QTextCursor cursor(doc);
        while (true) {
            cursor = doc->find(needle, cursor);
            if (cursor.isNull()) break;
            findMatches_.append(cursor.selectionStart());

            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(highlightColors_.find_match_bg);
            sel.cursor = cursor;
            findSelections_.append(sel);
        }
    }

    if (findMatches_.isEmpty()) {
        currentMatchIndex_ = -1;
    } else {
        int cursorPos = codeEditor_->textCursor().selectionStart();
        currentMatchIndex_ = 0;
        for (int i = 0; i < findMatches_.size(); i++) {
            if (findMatches_[i] >= cursorPos) { currentMatchIndex_ = i; break; }
        }
        selectMatch(currentMatchIndex_);
    }

    updateFindStatusLabel();
    refreshExtraSelections();
}

void MainWindow::selectMatch(int index) {
    if (index < 0 || index >= findMatches_.size()) return;
    QTextCursor cursor(codeEditor_->document());
    cursor.setPosition(findMatches_[index]);
    cursor.setPosition(findMatches_[index] + findInput_->text().length(), QTextCursor::KeepAnchor);
    codeEditor_->setTextCursor(cursor);
    codeEditor_->centerCursor();
}

void MainWindow::updateFindStatusLabel() {
    if (findMatches_.isEmpty())
        findStatusLabel_->setText(findInput_->text().isEmpty() ? "" : "No results");
    else
        findStatusLabel_->setText(QString("%1 of %2").arg(currentMatchIndex_ + 1).arg(findMatches_.size()));
}

void MainWindow::onFindNext() {
    if (findMatches_.isEmpty()) return;
    currentMatchIndex_ = (currentMatchIndex_ + 1) % findMatches_.size();
    selectMatch(currentMatchIndex_);
    updateFindStatusLabel();
}

void MainWindow::onFindPrev() {
    if (findMatches_.isEmpty()) return;
    currentMatchIndex_ = (currentMatchIndex_ - 1 + findMatches_.size()) % findMatches_.size();
    selectMatch(currentMatchIndex_);
    updateFindStatusLabel();
}

void MainWindow::onReplaceClicked() {
    if (findMatches_.isEmpty() || currentMatchIndex_ < 0) return;

    QTextCursor cursor(codeEditor_->document());
    cursor.setPosition(findMatches_[currentMatchIndex_]);
    cursor.setPosition(findMatches_[currentMatchIndex_] + findInput_->text().length(),
                        QTextCursor::KeepAnchor);
    cursor.insertText(replaceInput_->text());

    runFindSearch(); // positions shifted -- recompute and land on the next match
}

void MainWindow::onReplaceAllClicked() {
    QString needle = findInput_->text();
    if (needle.isEmpty()) return;
    QString replacement = replaceInput_->text();

    QTextDocument* doc = codeEditor_->document();
    QTextCursor editCursor(doc);
    editCursor.beginEditBlock();

    int count = 0;
    QTextCursor found = doc->find(needle);
    while (!found.isNull()) {
        found.insertText(replacement);
        count++;
        found = doc->find(needle, found.position());
    }
    editCursor.endEditBlock();

    runFindSearch();
    statusBar()->showMessage(QString("Replaced %1 occurrence(s)").arg(count));
}

// Editor helpers
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (completer_ && obj == completer_->popup() && event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab: {
            QModelIndex idx = completer_->popup()->currentIndex();
            if (idx.isValid())
                insertCompletion(idx.data().toString());
            completer_->popup()->hide();
            return true;
        }
        case Qt::Key_Escape:
            completer_->popup()->hide();
            return true;
        default:
            break;
        }
    }

    if ((obj == findInput_ || obj == replaceInput_) && event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape) {
            hideFindBar();
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (key->modifiers() & Qt::ShiftModifier) onFindPrev();
            else onFindNext();
            return true;
        }
    }

    if (obj == codeEditor_ && event->type() == QEvent::Wheel) {
        QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            adjustEditorZoom(wheel->angleDelta().y() > 0 ? 1 : -1);
            return true;
        }
    }

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

        // Auto-close (, [, {, " and skip over an already-present closer instead of
        // doubling it. Checked ahead of the Key_BraceRight dedent handler below so a
        // skip-over "}" takes priority over that block's plain-text-insert fallback.
        {
            QString typedText = key->text();
            if (typedText.size() == 1) {
                QChar tc = typedText.at(0);
                static const QString kOpen  = "([{\"";
                static const QString kClose = ")]}\"";

                if (kClose.contains(tc)) {
                    QTextCursor cursor = codeEditor_->textCursor();
                    if (!cursor.hasSelection() &&
                        codeEditor_->document()->characterAt(cursor.position()) == tc) {
                        cursor.movePosition(QTextCursor::NextCharacter);
                        codeEditor_->setTextCursor(cursor);
                        return true;
                    }
                }
                if (kOpen.contains(tc) && !codeEditor_->textCursor().hasSelection()) {
                    QChar closer = kClose.at(kOpen.indexOf(tc));
                    codeEditor_->insertPlainText(QString(tc) + QString(closer));
                    QTextCursor cursor = codeEditor_->textCursor();
                    cursor.movePosition(QTextCursor::PreviousCharacter);
                    codeEditor_->setTextCursor(cursor);
                    return true;
                }
            }
        }

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

        // These three have no QShortcut of their own (kept as raw key checks so
        // they only fire while the editor itself has focus), so they're matched
        // against keybindSeq_ directly instead of a hardcoded key+modifier pair.
        QKeySequence pressed(key->keyCombination());

        if (pressed == keybindSeq_.value("code_completion")) {
            showCompletionPopup();
            return true;
        }

        if (pressed == keybindSeq_.value("duplicate_line")) {
            QTextCursor cursor = codeEditor_->textCursor();
            int col = cursor.positionInBlock();

            QTextCursor lineCursor = cursor;
            lineCursor.movePosition(QTextCursor::StartOfLine);
            lineCursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            QString line = lineCursor.selectedText();

            cursor.movePosition(QTextCursor::EndOfLine);
            cursor.insertText("\n" + line);
            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, col);
            codeEditor_->setTextCursor(cursor);
            return true;
        }

        if (pressed == keybindSeq_.value("comment_toggle")) {
            toggleCommentSelection();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::showCompletionPopup() {
    static const QStringList kCompletionWords = {
        // GPIO / timing / interrupts
        "pinMode", "digitalWrite", "digitalRead", "analogWrite", "analogRead",
        "delay", "delayMicroseconds", "millis", "micros", "tone", "noTone",
        "attachInterrupt", "detachInterrupt", "noInterrupts", "interrupts",
        "digitalPinToInterrupt",
        // Serial
        "Serial.begin", "Serial.print", "Serial.println", "Serial.available", "Serial.read",
        "Serial1.begin", "Serial1.print", "Serial1.println",
        "Serial2.begin", "Serial2.print", "Serial2.println",
        // EEPROM
        "EEPROM.write", "EEPROM.read", "EEPROM.update",
        // Watchdog / sleep
        "wdt_enable", "wdt_disable", "wdt_reset",
        "set_sleep_mode", "sleep_enable", "sleep_disable", "sleep_cpu", "sleep_mode",
        "WDTO_15MS", "WDTO_30MS", "WDTO_60MS", "WDTO_120MS", "WDTO_250MS", "WDTO_500MS",
        "WDTO_1S", "WDTO_2S", "WDTO_4S", "WDTO_8S",
        "SLEEP_MODE_IDLE", "SLEEP_MODE_ADC", "SLEEP_MODE_PWR_DOWN",
        "SLEEP_MODE_PWR_SAVE", "SLEEP_MODE_STANDBY", "SLEEP_MODE_EXT_STANDBY",
        // Wire (I2C)
        "Wire.begin", "Wire.beginTransmission", "Wire.write", "Wire.endTransmission",
        "Wire.requestFrom", "Wire.available", "Wire.read",
        // SPI
        "SPI.begin", "SPI.transfer", "SPI.beginTransaction", "SPI.endTransaction",
        "SPISettings", "MSBFIRST", "LSBFIRST", "SPI_MODE0", "SPI_MODE1", "SPI_MODE2", "SPI_MODE3",
        // Servo
        "Servo", "attach", "write", "writeMicroseconds", "read", "detach", "attached",
        // LiquidCrystal
        "LiquidCrystal", "lcd.begin", "lcd.print", "lcd.setCursor", "lcd.clear",
        // SoftwareSerial
        "SoftwareSerial",
        // AVR direct registers
        "DDRB", "DDRC", "DDRD", "PORTB", "PORTC", "PORTD", "PINB", "PINC", "PIND",
        "TCCR1A", "TCCR1B", "TCNT1", "OCR1A", "OCR1B", "TIMSK1",
        "TCCR2A", "TCCR2B", "TCNT2", "OCR2A", "OCR2B", "TIMSK2",
        "ISR",
        // Pure computation (no hardware I/O, not rewritten by the preprocessor)
        "map", "constrain", "abs", "min", "max", "random", "randomSeed",
        "pulseIn", "shiftIn", "shiftOut",
        // Constants
        "HIGH", "LOW", "INPUT", "OUTPUT", "INPUT_PULLUP",
        "LED_BUILTIN", "A0", "A1", "A2", "A3", "A4", "A5",
        "CHANGE", "RISING", "FALLING",
        "PI", "TWO_PI", "HALF_PI",
        "true", "false",
        // Types / declarations
        "void", "int", "unsigned int", "long", "unsigned long", "float", "double",
        "char", "byte", "bool", "boolean", "String", "const", "static", "volatile",
        // Sketch skeleton
        "setup", "loop",
        // Control flow
        "if", "else", "for", "while", "do", "switch", "case", "break", "continue", "return",
        // Preprocessor directives -- WordUnderCursor doesn't include the '#',
        // so these match against the text typed after it (e.g. "def" -> "define")
        "define", "include", "ifdef", "ifndef", "endif", "pragma",
    };

    QStringList symbols = scanSketchSymbols();
    symbols += kCompletionWords;

    completer_->setModel(new QStringListModel(symbols, completer_));

    QTextCursor tc = codeEditor_->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    completer_->setCompletionPrefix(tc.selectedText());
    completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));

    QRect cr = codeEditor_->cursorRect();
    cr.setWidth(completer_->popup()->sizeHintForColumn(0)
                + completer_->popup()->verticalScrollBar()->sizeHint().width());
    completer_->complete(cr);
}

void MainWindow::clearCompileErrors() {
    compileSelections_.clear();
    refreshExtraSelections();
}

void MainWindow::showCompileErrors(const CompileResult& result) {
    compileSelections_.clear();

    for (const auto& err : result.errors) {
        int adjusted_line = err.line - result.header_lines;
        if (adjusted_line < 1) continue;
        QTextBlock block = codeEditor_->document()->findBlockByLineNumber(
            adjusted_line - 1);
        if (!block.isValid()) continue;

        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(err.is_error ? highlightColors_.error_bg : highlightColors_.warning_bg);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.format.setToolTip(QString::fromStdString(err.message));
        sel.cursor = QTextCursor(block);
        sel.cursor.select(QTextCursor::LineUnderCursor);
        compileSelections_.append(sel);
    }

    refreshExtraSelections();

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

void MainWindow::setAppTheme(bool dark) {
    darkTheme_ = dark;
    qApp->setPalette(appQPalette(dark));
    qApp->setStyleSheet(appStylesheet(dark));
    highlightColors_ = editorHighlightColors(dark);
    canvasWidget_->setDarkTheme(dark);
    if (highlighter_) highlighter_->setTheme(dark);
    if (signalTimeline_) signalTimeline_->setDarkTheme(dark);
    if (lineNumbers_) lineNumbers_->setDarkTheme(dark);
}

QKeySequence MainWindow::loadKeybind(QSettings& settings, const QString& id, QKeySequence def) {
    QString saved = settings.value("keybinds/" + id, QString()).toString();
    QKeySequence seq = saved.isEmpty() ? def : QKeySequence::fromString(saved);
    keybindSeq_[id] = seq;
    return seq;
}

// Persists every id -> sequence and, for ids backed by a live QShortcut,
// rebinds it immediately -- no restart needed to pick up a remap.
void MainWindow::applyKeybinds(const QMap<QString, QKeySequence>& newBinds) {
    QSettings settings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);
    for (auto it = newBinds.constBegin(); it != newBinds.constEnd(); ++it) {
        keybindSeq_[it.key()] = it.value();
        settings.setValue("keybinds/" + it.key(), it.value().toString());
        auto shortcut = keybindShortcuts_.find(it.key());
        if (shortcut != keybindShortcuts_.end())
            shortcut.value()->setKey(it.value());
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
    dialog.setAutoCompileOnSave(autoCompileOnSave_);
    dialog.setDarkTheme(darkTheme_);
    dialog.setDefaultSketchLocation(defaultSketchLocation_);
    dialog.setKeybinds(keybindSeq_);

    if (dialog.exec() == QDialog::Accepted) {
        compilerPath_      = dialog.compilerPath();
        projectRoot_       = dialog.projectRoot();
        int oldSerialCount = activeProfile_.serial_count;
        activeProfile_     = dialog.selectedBoard();
        analogNoise_       = dialog.analogNoise();
        autoCompileOnSave_ = dialog.autoCompileOnSave();
        defaultSketchLocation_ = dialog.defaultSketchLocation();
        settings.setValue("compiler/path", compilerPath_);
        settings.setValue("compiler/project_root", projectRoot_);
        settings.setValue("sketches/default_location", defaultSketchLocation_);
        settings.setValue("board/name", QString(activeProfile_.name));
        settings.setValue("simulation/analog_noise", analogNoise_);
        settings.setValue("editor/auto_compile_on_save", autoCompileOnSave_);
        darkTheme_ = dialog.darkTheme();
        settings.setValue("canvas/dark_theme", darkTheme_);
        setAppTheme(darkTheme_);
        applyKeybinds(dialog.keybinds());
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

    QString sketches_root = defaultSketchLocation_;
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
    codeEditor_->document()->setModified(false);
    currentSketchPath_ = file_path;
    canvasWidget_->loadLayout(file_path);
    windowTitleBase_ = "VEMCODE — " + name + ".cpp";
    updateWindowTitle();
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
            canvasWidget_->loadLayout(path);
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                codeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
                codeEditor_->document()->setModified(false);
                file.close();
            }
            windowTitleBase_ = "VEMCODE — " + QFileInfo(path).fileName();
            updateWindowTitle();
            checkForAutosaveRecovery(path);
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

    // Shared helper — extract any named void function's body by brace-counting
    auto extract_func_body = [&](const std::string& func_name) -> std::string {
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

    // Pre-extract loop and setup bodies for reuse across checks
    std::string loop_body  = extract_func_body("loop");
    std::string setup_body = extract_func_body("setup");

    // Pin out of range for selected board
    for (const auto& [name, pin] : pin_defs) {
        if (!is_pin_name(name)) continue;
        if (pin < 0 || pin >= 80) continue;
        if (pin >= activeProfile_.pin_count) {
            warnings << QString("WARNING: Pin %1 ('%2') is not available on the %3 (max pin %4)")
                .arg(pin)
                .arg(QString::fromStdString(name))
                .arg(activeProfile_.name)
                .arg(activeProfile_.pin_count - 1);
        }
    }

    // analogWrite() on a non-PWM pin
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

    // attachInterrupt() with raw interrupt number (Uno/Nano: interrupt 0=pin 2, 1=pin 3)
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

    // delay() inside an attachInterrupt callback function
    {
        std::regex ai_re(R"(attachInterrupt\s*\([^,]+,\s*(\w+)\s*,)");
        std::set<std::string> warned_funcs;
        for (auto it = std::sregex_iterator(src.begin(), src.end(), ai_re);
             it != std::sregex_iterator(); ++it) {
            std::string fn = (*it)[1].str();
            if (warned_funcs.count(fn)) continue;
            std::string body = extract_func_body(fn);
            if (!body.empty() && body.find("delay(") != std::string::npos) {
                warned_funcs.insert(fn);
                warnings << QString("WARNING: delay() inside '%1' (used as an interrupt handler) will hang on real Arduino — interrupts are disabled during ISR execution")
                    .arg(QString::fromStdString(fn));
            }
        }
    }

    // Pin defined as an expression (CircuitDetector cannot evaluate it)
    {
        std::regex def_re(R"(#\s*define\s+(\w+)\s+(.+))");
        for (auto it = std::sregex_iterator(src.begin(), src.end(), def_re);
             it != std::sregex_iterator(); ++it) {
            std::string name  = (*it)[1].str();
            std::string value = (*it)[2].str();
            auto comment = value.find("//");
            if (comment != std::string::npos) value = value.substr(0, comment);
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
                value.pop_back();
            bool has_op = value.find('+') != std::string::npos ||
                          value.find('*') != std::string::npos ||
                          value.find('/') != std::string::npos ||
                          value.find("<<") != std::string::npos;
            if (!has_op && value.size() > 1 && value.find('-') != std::string::npos)
                has_op = (value.find('-') != 0 &&
                          !(value.find('-') == 1 && value[0] == '('));
            if (!has_op) continue;
            if (!is_pin_name(name)) continue;
            warnings << QString("WARNING: Pin '%1' is defined as an expression — the simulator could not evaluate it and the component may not appear on the canvas; use a plain number instead")
                .arg(QString::fromStdString(name));
        }
    }

    // pinMode() never called for a digitalWrite() pin
    {
        std::set<int> dw_pins;
        std::set<int> pm_pins;
        std::regex dw_re(R"(digitalWrite\s*\(\s*(\w+))");
        std::regex pm_re(R"(pinMode\s*\(\s*(\w+))");
        auto collect_pins = [&](const std::regex& re, std::set<int>& out) {
            for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
                 it != std::sregex_iterator(); ++it) {
                std::string token = (*it)[1].str();
                int pin = -1;
                try { pin = std::stoi(token); } catch (...) {
                    auto def_it = pin_defs.find(token);
                    if (def_it != pin_defs.end()) pin = def_it->second;
                }
                if (pin >= 0) out.insert(pin);
            }
        };
        collect_pins(dw_re, dw_pins);
        collect_pins(pm_re, pm_pins);
        for (int pin : dw_pins) {
            if (!pm_pins.count(pin)) {
                warnings << QString("WARNING: Pin %1 is used with digitalWrite() but pinMode() was never called — it will default to INPUT on real hardware")
                    .arg(pin);
            }
        }
    }

    // Missing volatile on ISR-shared variables
    {
        // Find all variables written inside attachInterrupt callbacks
        std::set<std::string> isr_written;
        std::regex ai_re(R"(attachInterrupt\s*\([^,]+,\s*(\w+)\s*,)");
        for (auto it = std::sregex_iterator(src.begin(), src.end(), ai_re);
             it != std::sregex_iterator(); ++it) {
            std::string fn = (*it)[1].str();
            std::string body = extract_func_body(fn);
            if (body.empty()) continue;
            // Find assignments in ISR body: varName = or varName++/varName--
            std::regex assign_re(R"(\b(\w+)\s*(?:=|\+=|-=|\+\+|--))" );
            for (auto jt = std::sregex_iterator(body.begin(), body.end(), assign_re);
                 jt != std::sregex_iterator(); ++jt) {
                isr_written.insert((*jt)[1].str());
            }
        }
        // Check if any ISR-written variables appear in loop/setup without volatile
        for (const auto& var : isr_written) {
            if (var.empty()) continue;
            if (var.size() < 3) continue; // skip short names like i, n, x
            // Check loop and setup bodies for reads of this variable
            bool read_in_main = (loop_body.find(var) != std::string::npos ||
                                 setup_body.find(var) != std::string::npos);
            if (!read_in_main) continue;
            // Check if declared volatile anywhere in source
            std::regex vol_re(R"(\bvolatile\b[^;]*\b)" + var + R"(\b)");
            if (!std::regex_search(src, vol_re)) {
                warnings << QString("WARNING: '%1' is shared with an ISR but not declared volatile — this may work in simulation but will likely fail on real hardware")
                    .arg(QString::fromStdString(var));
            }
        }
    }

    // String += in a tight loop
    {
        if (!loop_body.empty()) {
            std::regex str_concat_re(R"(\bString\b[^;]*\+=)");
            if (std::regex_search(loop_body, str_concat_re)) {
                warnings << QString("WARNING: Repeated String concatenation in loop() causes heap fragmentation on real Arduino — consider using a char buffer instead");
            }
        }
    }

    return warnings;
}

QStringList MainWindow::scanSketchSymbols() {
    QStringList symbols;
    std::string src = codeEditor_->toPlainText().toStdString();

    auto collect = [&](const std::regex& re) {
        for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
            it != std::sregex_iterator(); ++it) {
            QString name = QString::fromStdString((*it)[1].str());
            if (!symbols.contains(name))
                symbols << name;
        }
    };

    collect(std::regex(R"(#\s*define\s+(\w+))"));                                   // constants
    collect(std::regex(R"(\b(?:void|int|float|double|bool|long|char|byte|String)\s+(\w+)\s*\()"));  // functions
    collect(std::regex(R"(\b(?:int|float|double|bool|long|char|byte|String)\s+(\w+)\s*[=;])"));      // variables

    return symbols;
}