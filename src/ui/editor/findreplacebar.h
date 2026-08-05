#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QList>
#include <QColor>

// Find/replace bar for the sketch editor. Hidden until showBar() is called (Ctrl+F).
class FindReplaceBar : public QWidget {
    Q_OBJECT

public:
    explicit FindReplaceBar(QPlainTextEdit* editor, QWidget* parent = nullptr);

    // Shows the bar, seeding the search text from the editor's current selection if any.
    void showBar();

    // Hides the bar and clears all match state/highlights.
    void hideBar();

    void setHighlightColor(QColor color);

signals:
    // Emitted whenever the match set changes; owner merges this into its own extra-selections layers.
    void matchesChanged(QList<QTextEdit::ExtraSelection> selections);

    // Replace All's result -- the owner has the QStatusBar, this widget doesn't.
    void statusMessage(const QString& text);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void runSearch();
    void selectMatch(int index);
    void updateStatusLabel();
    void onNext();
    void onPrev();
    void onReplaceClicked();
    void onReplaceAllClicked();

    QPlainTextEdit* editor_;
    QColor highlightColor_;

    QLineEdit* findInput_       = nullptr;
    QLineEdit* replaceInput_    = nullptr;
    QLabel*    findStatusLabel_ = nullptr;

    QList<int> findMatches_; // start position of each match in the document
    int        currentMatchIndex_ = -1;
};
