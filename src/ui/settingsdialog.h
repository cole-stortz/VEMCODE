#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    QString compilerPath() const;
    QString projectRoot() const;

    void setCompilerPath(const QString& path);
    void setProjectRoot(const QString& path);

private slots:
    void onBrowseCompiler();
    void onBrowseRoot();

private:
    QLineEdit* compilerEdit_;
    QLineEdit* rootEdit_;
};