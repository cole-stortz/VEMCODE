#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include "src/core/runtime/boardprofile.h"

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    QString compilerPath() const;
    QString projectRoot() const;
    BoardProfile selectedBoard() const;
    bool analogNoise() const;

    void setCompilerPath(const QString& path);
    void setProjectRoot(const QString& path);
    void setSelectedBoard(const QString& name);
    void setAnalogNoise(bool enabled);

    // Best-effort g++ + project-root detection, shared by GUI first-run and
    // headless mode. False if g++ isn't on PATH (or /usr/bin/g++) or
    // arduinoapi.h can't be found relative to the running binary.
    static bool autoDetectCompiler(QString& compilerPath, QString& projectRoot);

private slots:
    void onBrowseCompiler();
    void onBrowseRoot();
    void updateCompilerValidation();

private:
    QLineEdit* compilerEdit_;
    QLabel*    compilerValidLabel_;
    QLineEdit* rootEdit_;
    QComboBox* boardCombo_;
    QCheckBox* analogNoiseCheck_;
};