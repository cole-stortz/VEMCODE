#include "src/ui/mainwindow.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/componentitem.h"
#include "src/appsettings.h"
#include "src/ui/editor/sketchlinter.h"
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
#include <QRegularExpression>

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

    QSettings settings = appSettings();
    compilerPath_ = settings.value("compiler/path", "").toString();
    projectRoot_  = settings.value("compiler/project_root", "").toString();
    defaultSketchLocation_ = settings.value("sketches/default_location",
        QCoreApplication::applicationDirPath() + "/sketches").toString();

    QString boardName = settings.value("board/name", "Arduino Uno").toString();
    activeProfile_ = boardProfileForName(boardName);

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
            keybinds_.apply(dialog.keybinds());
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

    QShortcut* save_shortcut = new QShortcut(keybinds_.load(settings, "save", QKeySequence::Save), this);
    connect(save_shortcut, &QShortcut::activated, this, &MainWindow::onSaveClicked);
    keybinds_.registerShortcut("save", save_shortcut);
    QShortcut* save_as_shortcut = new QShortcut(keybinds_.load(settings, "save_as", QKeySequence::SaveAs), this);
    connect(save_as_shortcut, &QShortcut::activated, this, &MainWindow::onSaveAsClicked);
    keybinds_.registerShortcut("save_as", save_as_shortcut);

    // Mirrors runButton_'s own disabled-while-running guard -- a shortcut has
    // no disabled state of its own to stop it firing mid-run.
    QShortcut* run_shortcut = new QShortcut(keybinds_.load(settings, "run", QKeySequence(Qt::CTRL | Qt::Key_R)), this);
    connect(run_shortcut, &QShortcut::activated, this, [this]() {
        if (runButton_->isEnabled()) onRunClicked();
    });
    keybinds_.registerShortcut("run", run_shortcut);

    // Explicit key sequences instead of QKeySequence::ZoomIn/ZoomOut -- the platform
    // standard key for ZoomIn resolves to the same sequence as literal "Ctrl+=" on this
    // setup, and two QShortcuts sharing one sequence make Qt treat it as ambiguous
    // (neither fires). Bind both the unshifted "=" and shifted "+" explicitly instead;
    // only the unshifted one is user-remappable, the shifted one is a fixed convenience
    // alias so layouts where "+" is easier to reach than bare "=" still work.
    QShortcut* zoom_in_shortcut = new QShortcut(keybinds_.load(settings, "editor_zoom_in", QKeySequence(Qt::CTRL | Qt::Key_Equal)), this);
    connect(zoom_in_shortcut, &QShortcut::activated, this, [this]() { adjustEditorZoom(1); });
    keybinds_.registerShortcut("editor_zoom_in", zoom_in_shortcut);
    QShortcut* zoom_in_shortcut_shift = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Equal), this);
    connect(zoom_in_shortcut_shift, &QShortcut::activated, this, [this]() { adjustEditorZoom(1); });
    QShortcut* zoom_out_shortcut = new QShortcut(keybinds_.load(settings, "editor_zoom_out", QKeySequence(Qt::CTRL | Qt::Key_Minus)), this);
    connect(zoom_out_shortcut, &QShortcut::activated, this, [this]() { adjustEditorZoom(-1); });
    keybinds_.registerShortcut("editor_zoom_out", zoom_out_shortcut);
    QShortcut* zoom_reset_shortcut = new QShortcut(keybinds_.load(settings, "editor_zoom_reset", QKeySequence(Qt::CTRL | Qt::Key_0)), this);
    connect(zoom_reset_shortcut, &QShortcut::activated, this, &MainWindow::resetEditorZoom);
    keybinds_.registerShortcut("editor_zoom_reset", zoom_reset_shortcut);

    // Same unshifted/shifted pairing as the editor zoom shortcuts above, on Alt
    // instead of Ctrl so it doesn't collide with editor font zoom.
    QShortcut* canvas_zoom_in_shortcut = new QShortcut(keybinds_.load(settings, "canvas_zoom_in", QKeySequence(Qt::ALT | Qt::Key_Equal)), this);
    connect(canvas_zoom_in_shortcut, &QShortcut::activated, this, [this]() { canvasWidget_->zoomIn(); });
    keybinds_.registerShortcut("canvas_zoom_in", canvas_zoom_in_shortcut);
    QShortcut* canvas_zoom_in_shortcut_shift = new QShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal), this);
    connect(canvas_zoom_in_shortcut_shift, &QShortcut::activated, this, [this]() { canvasWidget_->zoomIn(); });
    QShortcut* canvas_zoom_out_shortcut = new QShortcut(keybinds_.load(settings, "canvas_zoom_out", QKeySequence(Qt::ALT | Qt::Key_Minus)), this);
    connect(canvas_zoom_out_shortcut, &QShortcut::activated, this, [this]() { canvasWidget_->zoomOut(); });
    keybinds_.registerShortcut("canvas_zoom_out", canvas_zoom_out_shortcut);

    connect(codeEditor_->document(), &QTextDocument::modificationChanged,
            this, [this](bool) { updateWindowTitle(); });

    QShortcut* find_shortcut = new QShortcut(keybinds_.load(settings, "find", QKeySequence::Find), this);
    connect(find_shortcut, &QShortcut::activated, findReplaceBar_, &FindReplaceBar::showBar);
    keybinds_.registerShortcut("find", find_shortcut);

    // code_completion, duplicate_line, and comment_toggle have no QShortcut of
    // their own -- they're raw key comparisons in EditorWithLines::keyPressEvent
    // (scoped to codeEditor_ only), so just seed keybindSeq_ with their current
    // sequence and push it down to the editor.
    keybinds_.load(settings, "code_completion", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space));
    keybinds_.load(settings, "duplicate_line", QKeySequence(Qt::CTRL | Qt::Key_D));
    keybinds_.load(settings, "comment_toggle", QKeySequence(Qt::CTRL | Qt::Key_Slash));
    codeEditor_->setActionKeybinds(keybinds_.value("code_completion"),
                                    keybinds_.value("duplicate_line"),
                                    keybinds_.value("comment_toggle"));
    connect(codeEditor_, &EditorWithLines::completionRequested,
            this, &MainWindow::showCompletionPopup);
    connect(codeEditor_, &EditorWithLines::dotTyped,
            this, &MainWindow::showMemberCompletionPopup);

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

// A clean exit with nothing unsaved means nothing to recover -- drop any
// pending autosave. If there ARE unsaved changes, leave it so
// checkForAutosaveRecovery can offer it back next time this sketch is opened.
void MainWindow::closeEvent(QCloseEvent* event) {
    bool has_real_path = !currentSketchPath_.isEmpty()
        && !currentSketchPath_.startsWith(QDir::tempPath());
    if (has_real_path && !codeEditor_->document()->isModified())
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

    // Find & Replace bar -- hidden until Ctrl+F
    findReplaceBar_ = new FindReplaceBar(codeEditor_);
    findReplaceBar_->setHighlightColor(highlightColors_.find_match_bg);
    connect(findReplaceBar_, &FindReplaceBar::matchesChanged, this,
            [this](QList<QTextEdit::ExtraSelection> selections) {
                findSelections_ = selections;
                refreshExtraSelections();
            });
    connect(findReplaceBar_, &FindReplaceBar::statusMessage, this,
            [this](const QString& text) { statusBar()->showMessage(text); });
    layout->addWidget(findReplaceBar_);

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
        if (!codeEditor_->hasFocus()) {
            idleCompletionTimer_->stop();
            return;
        }
        QTextCursor tc = codeEditor_->textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        QString word = tc.selectedText();

        // Popup already open (either the flat list or a member-narrowed one from
        // showMemberCompletionPopup) -- just refine its prefix as more is typed,
        // rather than re-triggering the idle timer or resetting the model.
        if (completer_->popup()->isVisible()) {
            idleCompletionTimer_->stop();
            completer_->setCompletionPrefix(word);
            if (completer_->completionCount() == 0)
                completer_->popup()->hide();
            else
                completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));
            return;
        }

        if (word.length() >= 3)
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
    serialPlotter_ = new SerialPlotter();
    debugTabs_->addTab(serialPlotter_, "Serial plotter");
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
    if (serialPlotter_) serialPlotter_->ingest(text, simTimer_.elapsed());
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
        case ComponentEventType::KeypadWiring: {
            QVariantList w = value.toList();
            int num_cols = w.value(0).toInt();
            std::vector<int> colPins, rowPins;
            int i = 1;
            for (int n = 0; n < num_cols; ++n) colPins.push_back(w.value(i++).toInt());
            for (; i < w.size(); ++i) rowPins.push_back(w.value(i).toInt());
            sketchThread_->injectKeypadWiring(colPins, rowPins);
            break;
        }
        case ComponentEventType::KeypadPress: {
            QVariantList k = value.toList();
            sketchThread_->injectKeypadPress(k.value(0).toInt(), k.value(1).toInt(), k.value(2).toInt() != 0);
            break;
        }
        case ComponentEventType::DhtReading: {
            QVariantList d = value.toList();
            sketchThread_->injectDht(pin, (float)d.value(0).toDouble(), (float)d.value(1).toDouble());
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
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to write: " + currentSketchPath_);
        runButton_->setEnabled(true);
        return;
    }
    file.write(codeEditor_->toPlainText().toUtf8());
    file.close();

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
    for (const QString& w : SketchLinter::checkSource(codeEditor_->toPlainText(), activeProfile_))
        serialMonitor_->appendPlainText(w + "\n");

    CompileResult result = compiler.compile(sketch_path);

    // Show pre-compile warnings (unsupported libraries, etc.) regardless of outcome
    for (const auto& w : result.warnings)
        serialMonitor_->appendPlainText(QString::fromStdString(w) + "\n");

    // Apply board hint from // @board comment if present
    if (!result.board_hint.empty()) {
        QString boardName = QString::fromStdString(result.board_hint);
        int oldSerialCount = activeProfile_.serial_count;
        bool knownBoard = boardProfileForName(boardName, activeProfile_);
        if (!knownBoard) {
            serialMonitor_->appendPlainText(
                "WARNING: Unknown board '" + boardName + "' in @board hint — using currently selected board instead.\n");
        }

        if (knownBoard) {
            canvasWidget_->setProfile(activeProfile_);
            boardLabel_->setText(activeProfile_.name);
            if (sketchThread_) sketchThread_->setProfile(activeProfile_);

            if (activeProfile_.serial_count != oldSerialCount)
                rebuildSerialMonitors();

            QSettings settings = appSettings();
            settings.setValue("board/name", boardName);
        }
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

        raw = SketchLinter::humanizeErrors(raw);
        serialMonitor_->appendPlainText("=== Compile errors ===\n");
        serialMonitor_->appendPlainText(raw);
        statusBar()->showMessage("Compile failed");
        runButton_->setEnabled(true);
        showCompileErrors(result);
        return;
    }

    showCompileErrors(result); // paints warning lines only -- no errors on this path
    signalTimeline_->clear();
    serialPlotter_->clear();
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
            if (SketchLinter::isPinName((*it)[1].str())) { has_pin_defines = true; break; }
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

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to open: " + path);
        return;
    }
    QString contents = QString::fromUtf8(file.readAll());
    file.close();

    currentSketchPath_ = path;
    canvasWidget_->loadLayout(path);
    codeEditor_->setPlainText(contents);
    codeEditor_->document()->setModified(false);

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

    if (obj == codeEditor_ && event->type() == QEvent::Wheel) {
        QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            adjustEditorZoom(wheel->angleDelta().y() > 0 ? 1 : -1);
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
        "Servo", "attach", "write", "read", "detach", "attached",
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

    QStringList symbols = SketchLinter::scanSymbols(codeEditor_->toPlainText());
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

void MainWindow::showMemberCompletionPopup() {
    // Only the globals/objects VEMCODE actually implements in simulation --
    // see Preprocessor::replace_api_calls (Serial/Serial1/Serial2/Wire/SPI/EEPROM)
    // and src/core/build/libs/*.inc (Servo/LiquidCrystal/SoftwareSerial) --
    // so this never suggests a real-Arduino method that would silently no-op here.
    static const QStringList kFixedGlobals = {
        "Serial", "Serial1", "Serial2", "Wire", "SPI", "EEPROM"
    };
    static const QMap<QString, QStringList> kMemberTables = {
        {"Serial",         {"begin", "print", "println", "printf", "available", "read"}},
        {"Serial1",        {"begin", "print", "println"}},
        {"Serial2",        {"begin", "print", "println"}},
        {"Wire",           {"begin", "beginTransmission", "write", "endTransmission",
                             "requestFrom", "available", "read"}},
        {"SPI",            {"begin", "beginTransaction", "endTransaction", "transfer"}},
        {"EEPROM",         {"read", "write", "update"}},
        {"Servo",          {"attach", "write", "read", "attached", "detach"}},
        {"LiquidCrystal",  {"begin", "clear", "setCursor", "write", "print", "createChar"}},
        {"SoftwareSerial", {"begin", "print", "println", "available", "read", "peek",
                             "write", "listen", "isListening", "overflow"}},
    };

    QTextCursor tc = codeEditor_->textCursor();
    QString before = tc.block().text().left(tc.positionInBlock());
    static const QRegularExpression receiverRe(R"(\b(\w+)\.$)");
    QRegularExpressionMatch m = receiverRe.match(before);
    if (!m.hasMatch())
        return;

    QString receiver = m.captured(1);
    QString type = kFixedGlobals.contains(receiver)
        ? receiver
        : SketchLinter::scanDeclaredTypes(codeEditor_->toPlainText()).value(receiver);
    if (!kMemberTables.contains(type))
        return;

    completer_->setModel(new QStringListModel(kMemberTables.value(type), completer_));
    completer_->setCompletionPrefix(QString());
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
    if (serialPlotter_) serialPlotter_->setDarkTheme(dark);
    if (lineNumbers_) lineNumbers_->setDarkTheme(dark);
    if (findReplaceBar_) findReplaceBar_->setHighlightColor(highlightColors_.find_match_bg);
}

void MainWindow::onSettingsClicked() {
    QSettings settings = appSettings();

    SettingsDialog dialog(this);
    dialog.setCompilerPath(compilerPath_);
    dialog.setProjectRoot(projectRoot_);
    dialog.setSelectedBoard(QString(activeProfile_.name));
    dialog.setAnalogNoise(analogNoise_);
    dialog.setAutoCompileOnSave(autoCompileOnSave_);
    dialog.setDarkTheme(darkTheme_);
    dialog.setDefaultSketchLocation(defaultSketchLocation_);
    dialog.setKeybinds(keybinds_.all());

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
        keybinds_.apply(dialog.keybinds());
        codeEditor_->setActionKeybinds(keybinds_.value("code_completion"),
                                        keybinds_.value("duplicate_line"),
                                        keybinds_.value("comment_toggle"));
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
    QSettings settings = appSettings();
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
    QSettings settings = appSettings();
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
