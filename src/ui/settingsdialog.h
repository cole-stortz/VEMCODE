#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
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

private slots:
    void onBrowseCompiler();
    void onBrowseRoot();

private:
    QLineEdit* compilerEdit_;
    QLineEdit* rootEdit_;
    QComboBox* boardCombo_;
    QCheckBox* analogNoiseCheck_;
};