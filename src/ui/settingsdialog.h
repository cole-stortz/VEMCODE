#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QKeySequenceEdit>
#include <QMap>
#include <QVector>
#include "src/core/runtime/boardprofile.h"

struct KeybindEntry {
    QString      id;    // settings key suffix, e.g. "run"
    QString      label; // shown in the Keybinds tab, e.g. "Run sketch"
    QKeySequence defaultSeq;
};

// Single source of truth for every remappable action's id/label/default --
// shared by MainWindow (to create/rebind QShortcuts) and SettingsDialog (to
// build the Keybinds tab), so the two can't drift out of sync.
const QVector<KeybindEntry>& defaultKeybinds();

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    QString compilerPath() const;
    QString projectRoot() const;
    BoardProfile selectedBoard() const;
    bool analogNoise() const;
    bool autoCompileOnSave() const;
    bool darkTheme() const;
    QString defaultSketchLocation() const;

    void setCompilerPath(const QString& path);
    void setProjectRoot(const QString& path);
    void setSelectedBoard(const QString& name);
    void setAnalogNoise(bool enabled);
    void setAutoCompileOnSave(bool enabled);
    void setDarkTheme(bool enabled);
    void setDefaultSketchLocation(const QString& path);

    // Overrides the Keybinds tab's rows (initialized to defaults in the
    // constructor) with the caller's actually-current sequences.
    void setKeybinds(const QMap<QString, QKeySequence>& current);
    QMap<QString, QKeySequence> keybinds() const;

    // Best-effort g++ + project-root detection, shared by GUI first-run and
    // headless mode. False if g++ isn't on PATH (or /usr/bin/g++) or
    // arduinoapi.h can't be found relative to the running binary.
    static bool autoDetectCompiler(QString& compilerPath, QString& projectRoot);

private slots:
    void onBrowseCompiler();
    void onBrowseRoot();
    void onBrowseSketchLocation();
    void updateCompilerValidation();
    void onResetKeybinds();
    void onAcceptClicked();

private:
    QWidget* buildGeneralTab();
    QWidget* buildKeybindsTab();

    QLineEdit* compilerEdit_;
    QLabel*    compilerValidLabel_;
    QLineEdit* rootEdit_;
    QComboBox* boardCombo_;
    QCheckBox* analogNoiseCheck_;
    QCheckBox* autoCompileCheck_;
    QCheckBox* darkThemeCheck_;
    QLineEdit* sketchLocationEdit_;

    QMap<QString, QKeySequenceEdit*> keybindEdits_;
};
