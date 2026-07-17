#include "src/ui/settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QCheckBox>
#include <QStandardPaths>
#include <QCoreApplication>
#include <filesystem>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("VEMCODE Settings");
    setMinimumWidth(500);

    QVBoxLayout* layout = new QVBoxLayout(this);

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

    // Save/Cancel buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    updateCompilerValidation();
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