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
#include "src/ui/signaltimeline.h"
#include "src/ui/codehighlighter.h"
#include "src/core/build/preprocessor.h"
#include "src/ui/linenumberarea.h"
#include "src/ui/variablewatch.h"
#include "src/ui/devicespanel.h"
#include "src/ui/spipanel.h"
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
    DevicesPanel*  devicesPanel_  = nullptr;
    SpiPanel*      spiPanel_      = nullptr;

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

private:
    void setupToolbar(QWidget* parent, QVBoxLayout* layout);
    void setupMainArea(QWidget* parent, QVBoxLayout* layout);
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    
    void showCompileErrors(const CompileResult& result);
    void clearCompileErrors();
    QStringList runStaticChecks(const QString& source);
    QStringList scanSketchSymbols();
    void showCompletionPopup();
    void updateWindowTitle();
    void adjustEditorZoom(int steps);
    void resetEditorZoom();
    void promptAndSaveAsNewSketch();
    void toggleCommentSelection();
    void refreshExtraSelections();
    void updateBracketMatch();
    void showFindBar();
    void hideFindBar();
    void runFindSearch();
    void selectMatch(int index);
    void updateFindStatusLabel();
    void onFindNext();
    void onFindPrev();
    void onReplaceClicked();
    void onReplaceAllClicked();
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
    QWidget*    findBar_          = nullptr;
    QWidget*    replaceRow_       = nullptr;
    QLineEdit*  findInput_        = nullptr;
    QLineEdit*  replaceInput_     = nullptr;
    QLabel*     findStatusLabel_  = nullptr;
    QList<int>  findMatches_; // start position of each match in the document
    int         currentMatchIndex_ = -1;
    
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
    BoardProfile activeProfile_ = BOARD_UNO;
    bool analogNoise_ = false;

    QSlider*   speedSlider_  = nullptr;
    QLineEdit* serialInput_  = nullptr;

    // Per-port line-start tracking for SoftwareSerial prefix insertion
    QMap<int, bool> softSerialLineStart_;
};