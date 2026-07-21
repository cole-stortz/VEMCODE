#pragma once
#include <QPlainTextEdit>
#include <QWidget>
#include <QPainter>
#include <QTextBlock>
#include <QKeySequence>

// The sketch editor widget. Exposes protected QPlainTextEdit methods needed
// by LineNumberArea, and owns editor-only key handling (tab-insert,
// auto-indent, bracket auto-close/skip, dedent, duplicate-line,
// comment-toggle) that only makes sense while the editor itself has focus.
class EditorWithLines : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit EditorWithLines(QWidget* parent = nullptr)
        : QPlainTextEdit(parent) {}

    QTextBlock  firstBlock()    { return firstVisibleBlock(); }
    QRectF      blockGeo(const QTextBlock& b) { return blockBoundingGeometry(b); }
    QRectF      blockRect(const QTextBlock& b){ return blockBoundingRect(b); }
    QPointF     contentOff()    { return contentOffset(); }
    void setLeftMargin(int margin) { setViewportMargins(margin, 0, 0, 0); }

    // code_completion/duplicate_line/comment_toggle have no QShortcut of
    // their own (kept as raw key comparisons so they only fire while the
    // editor itself has focus) -- owned by MainWindow's KeybindManager,
    // pushed in here whenever they're loaded or remapped.
    void setActionKeybinds(QKeySequence completion, QKeySequence duplicateLine, QKeySequence commentToggle) {
        completionKey_    = completion;
        duplicateLineKey_ = duplicateLine;
        commentToggleKey_ = commentToggle;
    }

signals:
    // completion has no self-contained handling here -- it needs MainWindow's
    // QCompleter, so this just asks MainWindow to show the popup.
    void completionRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    // Toggles "// " on every line touched by the selection (or just the
    // current line with no selection). Uncomments if every non-blank line in
    // range already starts with "//", otherwise comments every non-blank line.
    void toggleCommentSelection();

    QKeySequence completionKey_;
    QKeySequence duplicateLineKey_;
    QKeySequence commentToggleKey_;
};

class LineNumberArea : public QWidget {
    Q_OBJECT

public:
    explicit LineNumberArea(EditorWithLines* editor)
        : QWidget(editor), editor_(editor)
    {
        connect(editor_, &QPlainTextEdit::blockCountChanged,
                this, &LineNumberArea::updateWidth);
        connect(editor_, &QPlainTextEdit::updateRequest,
                this, &LineNumberArea::updateContents);
        updateWidth();
    }

    int requiredWidth() const {
        int digits = 1;
        int count  = qMax(1, editor_->blockCount());
        while (count >= 10) { count /= 10; digits++; }
        return 12 + digits * QFontMetrics(editor_->font()).horizontalAdvance('9');
    }

    void setDarkTheme(bool dark) { dark_ = dark; update(); }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter p(this);
        p.fillRect(event->rect(), dark_ ? QColor("#252526") : QColor("#e8e8ec"));

        QTextBlock block  = editor_->firstBlock();
        int block_number  = block.blockNumber();
        QRectF offset     = editor_->blockGeo(block).translated(editor_->contentOff());
        qreal top         = offset.top();
        qreal bottom      = top + editor_->blockRect(block).height();

        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                p.setPen(dark_ ? QColor("#555") : QColor("#8a8a92"));
                p.setFont(editor_->font());
                p.drawText(0, (int)top, width() - 4,
                           (int)editor_->fontMetrics().height(),
                           Qt::AlignRight,
                           QString::number(block_number + 1));
            }
            block        = block.next();
            top          = bottom;
            bottom       = top + editor_->blockRect(block).height();
            block_number++;
        }
    }

private slots:
    void updateWidth() {
        editor_->setLeftMargin(requiredWidth());
        setGeometry(0, 0, requiredWidth(), editor_->height());
    }

    void updateContents(const QRect& rect, int dy) {
        if (dy) scroll(0, dy);
        else    update(0, rect.y(), width(), rect.height());
        updateWidth();
    }

private:
    EditorWithLines* editor_;
    bool dark_ = true;
};