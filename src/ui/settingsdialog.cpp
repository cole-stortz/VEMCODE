#include "src/ui/settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QCheckBox>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTabWidget>
#include <QScrollArea>
#include <QMessageBox>
#include <filesystem>

const QVector<KeybindEntry>& defaultKeybinds() {
    static const QVector<KeybindEntry> kDefs = {
        { "save",              "Save sketch",         QKeySequence::Save },
        { "save_as",           "Save As",              QKeySequence::SaveAs },
        { "run",                "Run sketch",           QKeySequence(Qt::CTRL | Qt::Key_R) },
        { "editor_zoom_in",     "Editor zoom in",       QKeySequence(Qt::CTRL | Qt::Key_Equal) },
        { "editor_zoom_out",    "Editor zoom out",      QKeySequence(Qt::CTRL | Qt::Key_Minus) },
        { "editor_zoom_reset",  "Reset editor zoom",    QKeySequence(Qt::CTRL | Qt::Key_0) },
        { "canvas_zoom_in",     "Canvas zoom in",       QKeySequence(Qt::ALT | Qt::Key_Equal) },
        { "canvas_zoom_out",    "Canvas zoom out",      QKeySequence(Qt::ALT | Qt::Key_Minus) },
        { "find",               "Find",                 QKeySequence::Find },
        { "code_completion",    "Code completion",      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space) },
        { "duplicate_line",     "Duplicate line",       QKeySequence(Qt::CTRL | Qt::Key_D) },
        { "comment_toggle",     "Toggle comment",       QKeySequence(Qt::CTRL | Qt::Key_Slash) },
    };
    return kDefs;
}

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("VEMCODE Settings");
    setMinimumSize(520, 420);

    QVBoxLayout* outer = new QVBoxLayout(this);

    QTabWidget* tabs = new QTabWidget(this);
    tabs->addTab(buildGeneralTab(), "General");
    tabs->addTab(buildKeybindsTab(), "Keybinds");
    outer->addWidget(tabs);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAcceptClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    updateCompilerValidation();
}

QWidget* SettingsDialog::buildGeneralTab() {
    QWidget*     tab    = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);

    // Compiler path row
    QHBoxLayout* compilerRow = new QHBoxLayout();
    compilerRow->addWidget(new QLabel("Compiler path:"));
    compilerEdit_ = new QLineEdit();
    compilerEdit_->setPlaceholderText("g++");
    connect(compilerEdit_, &QLineEdit::textChanged, this, &SettingsDialog::updateCompilerValidation);
    compilerRow->addWidget(compilerEdit_);
    compilerValidLabel_ = new QLabel();
    compilerValidLabel_->setFixedWidth(16);
    compilerRow->addWidget(compilerValidLabel_);
    QPushButton* browseCompiler = new QPushButton("Browse");
    connect(browseCompiler, &QPushButton::clicked, this, &SettingsDialog::onBrowseCompiler);
    compilerRow->addWidget(browseCompiler);
    layout->addLayout(compilerRow);

    // Project root row
    QHBoxLayout* rootRow = new QHBoxLayout();
    rootRow->addWidget(new QLabel("Project root:"));
    rootEdit_ = new QLineEdit();
    rootEdit_->setPlaceholderText("C:/Users/.../VEMCODE");
    rootRow->addWidget(rootEdit_);
    QPushButton* browseRoot = new QPushButton("Browse");
    connect(browseRoot, &QPushButton::clicked, this, &SettingsDialog::onBrowseRoot);
    rootRow->addWidget(browseRoot);
    layout->addLayout(rootRow);

    // Default sketch location row
    QHBoxLayout* sketchLocationRow = new QHBoxLayout();
    sketchLocationRow->addWidget(new QLabel("Default sketch location:"));
    sketchLocationEdit_ = new QLineEdit();
    sketchLocationEdit_->setPlaceholderText("Where New Sketch / Save create sketch folders");
    sketchLocationRow->addWidget(sketchLocationEdit_);
    QPushButton* browseSketchLocation = new QPushButton("Browse");
    connect(browseSketchLocation, &QPushButton::clicked, this, &SettingsDialog::onBrowseSketchLocation);
    sketchLocationRow->addWidget(browseSketchLocation);
    layout->addLayout(sketchLocationRow);

    // Board selector row
    QHBoxLayout* boardRow = new QHBoxLayout();
    boardRow->addWidget(new QLabel("Board:"));
    boardCombo_ = new QComboBox();
    boardCombo_->addItem("Arduino Uno");
    boardCombo_->addItem("Arduino Nano");
    boardCombo_->addItem("Arduino Mega 2560");
    boardCombo_->addItem("Arduino Due");
    boardCombo_->addItem("Teensy 4.1");
    boardRow->addWidget(boardCombo_);
    layout->addLayout(boardRow);

    // Simulation options
    analogNoiseCheck_ = new QCheckBox("Analog noise (gaussian ±2 ADC counts on analogRead)");
    layout->addWidget(analogNoiseCheck_);

    // Editor options
    autoCompileCheck_ = new QCheckBox("Auto-compile on save (Ctrl+R still runs manually)");
    layout->addWidget(autoCompileCheck_);

    // Canvas options
    darkThemeCheck_ = new QCheckBox("Dark theme");
    darkThemeCheck_->setChecked(true);
    layout->addWidget(darkThemeCheck_);

    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::buildKeybindsTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(tab);

    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    QWidget* rowsWidget = new QWidget();
    QFormLayout* rows = new QFormLayout(rowsWidget);

    for (const KeybindEntry& entry : defaultKeybinds()) {
        QKeySequenceEdit* edit = new QKeySequenceEdit(entry.defaultSeq);
        keybindEdits_[entry.id] = edit;
        rows->addRow(entry.label + ":", edit);
    }

    scroll->setWidget(rowsWidget);
    outer->addWidget(scroll);

    QLabel* hint = new QLabel("Click a field and press a key combination. Backspace clears it.");
    hint->setProperty("role", "muted-label");
    outer->addWidget(hint);

    QPushButton* resetButton = new QPushButton("Reset to Defaults");
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::onResetKeybinds);
    outer->addWidget(resetButton);

    return tab;
}

QString SettingsDialog::compilerPath() const {
    return compilerEdit_->text();
}

QString SettingsDialog::projectRoot() const {
    return rootEdit_->text();
}

void SettingsDialog::setCompilerPath(const QString& path) {
    compilerEdit_->setText(path);
}

void SettingsDialog::setProjectRoot(const QString& path) {
    rootEdit_->setText(path);
}

QString SettingsDialog::defaultSketchLocation() const {
    return sketchLocationEdit_->text();
}

void SettingsDialog::setDefaultSketchLocation(const QString& path) {
    sketchLocationEdit_->setText(path);
}

BoardProfile SettingsDialog::selectedBoard() const {
    QString name = boardCombo_->currentText();
    if (name == "Arduino Nano")       return BOARD_NANO;
    if (name == "Arduino Mega 2560")  return BOARD_MEGA;
    if (name == "Arduino Due")        return BOARD_DUE;
    if (name == "Teensy 4.1")         return BOARD_TEENSY;
    return BOARD_UNO;
}

void SettingsDialog::setSelectedBoard(const QString& name) {
    int i = boardCombo_->findText(name);
    if (i >= 0) boardCombo_->setCurrentIndex(i);
}

bool SettingsDialog::analogNoise() const {
    return analogNoiseCheck_->isChecked();
}

void SettingsDialog::setAnalogNoise(bool enabled) {
    analogNoiseCheck_->setChecked(enabled);
}

bool SettingsDialog::autoCompileOnSave() const {
    return autoCompileCheck_->isChecked();
}

void SettingsDialog::setAutoCompileOnSave(bool enabled) {
    autoCompileCheck_->setChecked(enabled);
}

bool SettingsDialog::darkTheme() const {
    return darkThemeCheck_->isChecked();
}

void SettingsDialog::setDarkTheme(bool enabled) {
    darkThemeCheck_->setChecked(enabled);
}

void SettingsDialog::setKeybinds(const QMap<QString, QKeySequence>& current) {
    for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
        auto edit = keybindEdits_.find(it.key());
        if (edit != keybindEdits_.end())
            edit.value()->setKeySequence(it.value());
    }
}

QMap<QString, QKeySequence> SettingsDialog::keybinds() const {
    QMap<QString, QKeySequence> result;
    for (auto it = keybindEdits_.constBegin(); it != keybindEdits_.constEnd(); ++it)
        result[it.key()] = it.value()->keySequence();
    return result;
}

void SettingsDialog::onResetKeybinds() {
    for (const KeybindEntry& entry : defaultKeybinds())
        keybindEdits_[entry.id]->setKeySequence(entry.defaultSeq);
}

void SettingsDialog::onAcceptClicked() {
    QMap<QString, QStringList> byKeySequence; // sequence text -> labels using it
    QMap<QString, QString> labels;
    for (const KeybindEntry& entry : defaultKeybinds())
        labels[entry.id] = entry.label;

    for (auto it = keybindEdits_.constBegin(); it != keybindEdits_.constEnd(); ++it) {
        QKeySequence seq = it.value()->keySequence();
        if (seq.isEmpty()) continue;
        byKeySequence[seq.toString()].append(labels.value(it.key()));
    }

    QStringList conflicts;
    for (auto it = byKeySequence.constBegin(); it != byKeySequence.constEnd(); ++it)
        if (it.value().size() > 1)
            conflicts << it.key() + " -- " + it.value().join(", ");

    if (!conflicts.isEmpty()) {
        QMessageBox::warning(this, "Keybind conflict",
            "The same shortcut is assigned to more than one action:\n\n" + conflicts.join("\n"));
        return;
    }

    accept();
}

void SettingsDialog::onBrowseCompiler() {
    QString path = QFileDialog::getOpenFileName(
        this, "Select g++ compiler", QString(), "Executable (*.exe)");
    if (!path.isEmpty()) compilerEdit_->setText(path);
}

void SettingsDialog::onBrowseRoot() {
    QString path = QFileDialog::getExistingDirectory(
        this, "Select VEMCODE project root");
    if (!path.isEmpty()) rootEdit_->setText(path);
}

void SettingsDialog::onBrowseSketchLocation() {
    QString path = QFileDialog::getExistingDirectory(
        this, "Select default sketch location");
    if (!path.isEmpty()) sketchLocationEdit_->setText(path);
}

void SettingsDialog::updateCompilerValidation() {
    QFileInfo info(compilerEdit_->text());
    bool valid = info.exists() && info.isFile();
    compilerValidLabel_->setText(valid ? "✓" : "✗");
    compilerValidLabel_->setStyleSheet(valid ? "color: #2d9d2d;" : "color: #b03030;");
}

// Repo root is always the parent of the app/ dir the binary runs from -- no
// need to search for it, unlike the compiler.
bool SettingsDialog::autoDetectCompiler(QString& compilerPath, QString& projectRoot) {
    QString found = QStandardPaths::findExecutable("g++");
#ifndef _WIN32
    if (found.isEmpty() && std::filesystem::exists("/usr/bin/g++"))
        found = "/usr/bin/g++";
#endif
    if (found.isEmpty()) return false;

    std::filesystem::path root =
        std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()).parent_path();
    if (!std::filesystem::exists(root / "src" / "core" / "runtime" / "arduinoapi.h"))
        return false;

    compilerPath = found;
    projectRoot  = QString::fromStdString(root.string());
    return true;
}
