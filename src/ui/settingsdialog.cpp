#include "src/ui/settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("VirtualBench Settings");
    setMinimumWidth(500);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // Compiler path row
    QHBoxLayout* compilerRow = new QHBoxLayout();
    compilerRow->addWidget(new QLabel("Compiler path:"));
    compilerEdit_ = new QLineEdit();
    compilerEdit_->setPlaceholderText("C:/Qt/Tools/mingw1310_64/bin/g++.exe");
    compilerRow->addWidget(compilerEdit_);
    QPushButton* browseCompiler = new QPushButton("Browse");
    connect(browseCompiler, &QPushButton::clicked, this, &SettingsDialog::onBrowseCompiler);
    compilerRow->addWidget(browseCompiler);
    layout->addLayout(compilerRow);

    // Project root row
    QHBoxLayout* rootRow = new QHBoxLayout();
    rootRow->addWidget(new QLabel("Project root:"));
    rootEdit_ = new QLineEdit();
    rootEdit_->setPlaceholderText("C:/Users/.../VirtualBench");
    rootRow->addWidget(rootEdit_);
    QPushButton* browseRoot = new QPushButton("Browse");
    connect(browseRoot, &QPushButton::clicked, this, &SettingsDialog::onBrowseRoot);
    rootRow->addWidget(browseRoot);
    layout->addLayout(rootRow);

    // Save/Cancel buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
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

void SettingsDialog::onBrowseCompiler() {
    QString path = QFileDialog::getOpenFileName(
        this, "Select g++ compiler", QString(), "Executable (*.exe)");
    if (!path.isEmpty()) compilerEdit_->setText(path);
}

void SettingsDialog::onBrowseRoot() {
    QString path = QFileDialog::getExistingDirectory(
        this, "Select VirtualBench project root");
    if (!path.isEmpty()) rootEdit_->setText(path);
}