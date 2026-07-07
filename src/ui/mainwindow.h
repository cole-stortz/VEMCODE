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
#include <QScrollBar>
#include <qcombobox.h>
#include "src/core/host/sketchhostthread.h"
#include "src/core/build/compiler.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/circuitdetector.h"
#include "src/ui/signaltimeline.h"
#include "src/ui/codehighlighter.h"
#include "src/core/build/preprocessor.h"
#include "src/ui/linenumberarea.h"
#include "src/ui/variablewatch.h"
#include "src/ui/settingsdialog.h"
#include "src/core/runtime/boardprofile.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    SignalTimeline* signalTimeline_ = nullptr;
    QElapsedTimer   simTimer_;
    VariableWatch* variableWatch_ = nullptr;

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
    void onNewSketch();
    void onRecentSketches();
    void addToRecentSketches(const QString& path);
    void onSpeedChanged(int value);
    void onSerialSend();
    void insertCompletion(const QString& completion);

private:
    void setupToolbar(QWidget* parent, QVBoxLayout* layout);
    void setupMainArea(QWidget* parent, QVBoxLayout* layout);
    bool eventFilter(QObject* obj, QEvent* event) override;
    
    void showCompileErrors(const CompileResult& result);
    void clearCompileErrors();
    QStringList runStaticChecks(const QString& source);
    QStringList scanSketchSymbols();

    QWidget* buildEditorPanel();
    QWidget* buildCanvasPanel();
    QWidget* buildDebugPanel();
    QWidget* buildSerialPanel();
    void     rebuildSerialMonitors();

    // Toolbar
    QPushButton*    runButton_      = nullptr;
    QPushButton*    stopButton_     = nullptr;
    QLabel*         boardLabel_     = nullptr;

    // Editor panel (left)
    EditorWithLines* codeEditor_  = nullptr;
    CodeHighlighter* highlighter_ = nullptr;
    LineNumberArea* lineNumbers_  = nullptr;
    QCompleter* completer_        = nullptr;
    
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

    // Circuit detection
    CircuitDetector    detector_;

    // Settings
    QString compilerPath_;
    QString projectRoot_;
    BoardProfile activeProfile_ = BOARD_UNO;
    bool analogNoise_ = false;

    QSlider*   speedSlider_  = nullptr;
    QLineEdit* serialInput_  = nullptr;

    // Per-port line-start tracking for SoftwareSerial prefix insertion
    QMap<int, bool> softSerialLineStart_;
};