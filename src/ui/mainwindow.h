#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QFile>
#include <QElapsedTimer>
#include <QDir>
#include <QShortcut>
#include <QSettings>
#include <QSlider>
#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QTimer>
#include <QScrollBar>
#include <QCloseEvent>
#include <qcombobox.h>
#include "src/core/host/sketchhostthread.h"
#include "src/core/build/compiler.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/circuitdetector.h"
#include "src/ui/panels/signaltimeline.h"
#include "src/ui/panels/serialplotter.h"
#include "src/ui/editor/codehighlighter.h"
#include "src/core/build/preprocessor.h"
#include "src/ui/editor/linenumberarea.h"
#include "src/ui/panels/variablewatch.h"
#include "src/ui/panels/devicespanel.h"
#include "src/ui/panels/spipanel.h"
#include "src/ui/settingsdialog.h"
#include "src/ui/apptheme.h"
#include "src/core/runtime/boardprofile.h"
#include "src/ui/editor/keybindmanager.h"
#include "src/ui/editor/findreplacebar.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    SignalTimeline* signalTimeline_ = nullptr;
    QElapsedTimer   simTimer_;
    VariableWatch* variableWatch_ = nullptr;
    DevicesPanel*  devicesPanel_  = nullptr;
    SpiPanel*      spiPanel_      = nullptr;
    SerialPlotter* serialPlotter_ = nullptr;

private slots:
    void onSerialOutput(QString text);
    void onSerial1Output(QString text);
    void onSerial2Output(QString text);
    void onSoftSerialOutput(int rxPin, QString text);
    void onPinChanged(int pin, int value);
    void onComponentInput(int pin, int eventType, QVariant value);
    void onSketchReloaded();
    void onLoadFailed(QString reason);
    void onRunClicked();
    void onStopClicked();
    void onOpenClicked();
    void onSettingsClicked();
    void onSaveClicked();
    void onSaveAsClicked();
    void onNewSketch();
    void onRecentSketches();
    void addToRecentSketches(const QString& path);
    void onSpeedChanged(int value);
    void onSerialSend();
    void insertCompletion(const QString& completion);
    void onLayoutToggled(bool on);

private:
    void setupToolbar(QWidget* parent, QVBoxLayout* layout);
    void setupMainArea(QWidget* parent, QVBoxLayout* layout);
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    
    void showCompileErrors(const CompileResult& result);
    void clearCompileErrors();
    void showCompletionPopup();
    void showMemberCompletionPopup();
    void updateWindowTitle();
    void adjustEditorZoom(int steps);
    void resetEditorZoom();
    void promptAndSaveAsNewSketch();
    void refreshExtraSelections();
    void updateBracketMatch();
    void checkForAutosaveRecovery(const QString& sketchPath);

    QWidget* buildEditorPanel();
    QWidget* buildCanvasPanel();
    QWidget* buildDebugPanel();
    QWidget* buildSerialPanel();
    void     rebuildSerialMonitors();

    // Toolbar
    QPushButton*    runButton_      = nullptr;
    QPushButton*    stopButton_     = nullptr;
    QLabel*         boardLabel_     = nullptr;
    QPushButton*    layoutButton_   = nullptr;

    // Editor panel (left)
    EditorWithLines* codeEditor_  = nullptr;
    CodeHighlighter* highlighter_ = nullptr;
    LineNumberArea* lineNumbers_  = nullptr;
    QCompleter* completer_        = nullptr;
    QTimer*     idleCompletionTimer_ = nullptr;
    QTimer*     autosaveTimer_       = nullptr;

    // Editor extra-selection layers -- merged together in refreshExtraSelections()
    // since QPlainTextEdit::setExtraSelections() replaces the whole list each call
    QList<QTextEdit::ExtraSelection> compileSelections_;
    QList<QTextEdit::ExtraSelection> bracketSelections_;
    QList<QTextEdit::ExtraSelection> findSelections_;

    // Find & Replace bar
    FindReplaceBar* findReplaceBar_ = nullptr;

    // Canvas panel (top right)
    CanvasWidget*   canvasWidget_   = nullptr;

    // Debug panel (bottom right)
    QTabWidget*     debugTabs_       = nullptr;
    QPlainTextEdit* serialMonitor_   = nullptr;
    QPlainTextEdit* serialMonitor1_  = nullptr;
    QPlainTextEdit* serialMonitor2_  = nullptr;

    // Simulation
    SketchThread*      sketchThread_  = nullptr;
    QString            currentSketchPath_;

    // Window title base (sans "unsaved changes" marker) and editor zoom level
    QString windowTitleBase_ = "VEMCODE";
    int     editorZoomLevel_ = 0;

    // Circuit detection
    CircuitDetector    detector_;

    // Settings
    QString compilerPath_;
    QString projectRoot_;
    QString defaultSketchLocation_;
    BoardProfile activeProfile_ = BOARD_UNO;
    bool analogNoise_ = false;
    bool autoCompileOnSave_ = false;
    bool darkTheme_ = true;
    EditorHighlightColors highlightColors_ = editorHighlightColors(true);
    void setAppTheme(bool dark); // qApp stylesheet + canvas + syntax highlighter + signal timeline

    // Falls back to defaultKeybinds() (settingsdialog.h) for any id with no
    // saved override yet.
    KeybindManager keybinds_;

    QSlider*   speedSlider_  = nullptr;
    QLineEdit* serialInput_  = nullptr;

    // Per-port line-start tracking for SoftwareSerial prefix insertion
    QMap<int, bool> softSerialLineStart_;
};