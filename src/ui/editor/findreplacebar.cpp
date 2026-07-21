#include "src/ui/editor/findreplacebar.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QKeyEvent>
#include <QTextDocument>

FindReplaceBar::FindReplaceBar(QPlainTextEdit* editor, QWidget* parent)
    : QWidget(parent), editor_(editor)
{
    setObjectName("findBar");
    setProperty("role", "panel-header"); // same bg/border look as panel headers

    QVBoxLayout* findBarLayout = new QVBoxLayout(this);
    findBarLayout->setContentsMargins(8, 6, 8, 6);
    findBarLayout->setSpacing(4);

    QHBoxLayout* findRowLayout = new QHBoxLayout();
    findRowLayout->setSpacing(6);
    findInput_ = new QLineEdit(this);
    findInput_->setPlaceholderText("Find");
    findRowLayout->addWidget(findInput_);

    findStatusLabel_ = new QLabel(this);
    findStatusLabel_->setProperty("role", "muted-label");
    findStatusLabel_->setFixedWidth(60);
    findRowLayout->addWidget(findStatusLabel_);

    QPushButton* findPrevButton = new QPushButton("Prev", this);
    findPrevButton->setFixedHeight(24);
    findPrevButton->setProperty("role", "outline");
    connect(findPrevButton, &QPushButton::clicked, this, &FindReplaceBar::onPrev);
    findRowLayout->addWidget(findPrevButton);

    QPushButton* findNextButton = new QPushButton("Next", this);
    findNextButton->setFixedHeight(24);
    findNextButton->setProperty("role", "outline");
    connect(findNextButton, &QPushButton::clicked, this, &FindReplaceBar::onNext);
    findRowLayout->addWidget(findNextButton);

    QPushButton* findCloseButton = new QPushButton("✕", this);
    findCloseButton->setFixedSize(24, 24);
    findCloseButton->setProperty("role", "outline");
    connect(findCloseButton, &QPushButton::clicked, this, &FindReplaceBar::hideBar);
    findRowLayout->addWidget(findCloseButton);

    findBarLayout->addLayout(findRowLayout);

    QWidget* replaceRow = new QWidget(this);
    QHBoxLayout* replaceRowLayout = new QHBoxLayout(replaceRow);
    replaceRowLayout->setContentsMargins(0, 0, 0, 0);
    replaceRowLayout->setSpacing(6);
    replaceInput_ = new QLineEdit(replaceRow);
    replaceInput_->setPlaceholderText("Replace");
    replaceRowLayout->addWidget(replaceInput_);

    QPushButton* replaceButton = new QPushButton("Replace", replaceRow);
    replaceButton->setFixedHeight(24);
    replaceButton->setProperty("role", "outline");
    connect(replaceButton, &QPushButton::clicked, this, &FindReplaceBar::onReplaceClicked);
    replaceRowLayout->addWidget(replaceButton);

    QPushButton* replaceAllButton = new QPushButton("Replace All", replaceRow);
    replaceAllButton->setFixedHeight(24);
    replaceAllButton->setProperty("role", "outline");
    connect(replaceAllButton, &QPushButton::clicked, this, &FindReplaceBar::onReplaceAllClicked);
    replaceRowLayout->addWidget(replaceAllButton);

    findBarLayout->addWidget(replaceRow);

    connect(findInput_, &QLineEdit::textChanged, this, [this](const QString&) { runSearch(); });
    findInput_->installEventFilter(this);
    replaceInput_->installEventFilter(this);

    // Keeps findMatches_ correct if the document is edited while the bar is
    // open -- previously this only re-ran on findInput_ changes or after our
    // own replace actions, so typing directly in the editor with the bar
    // open left findMatches_ pointing at stale positions.
    connect(editor_, &QPlainTextEdit::textChanged, this, [this]() {
        if (isVisible()) runSearch();
    });

    hide();
}

void FindReplaceBar::setHighlightColor(QColor color) {
    highlightColor_ = color;
}

void FindReplaceBar::showBar() {
    show();

    QString selected = editor_->textCursor().selectedText();
    if (!selected.isEmpty() && !selected.contains(QChar::ParagraphSeparator))
        findInput_->setText(selected);

    findInput_->setFocus();
    findInput_->selectAll();
    runSearch();
}

void FindReplaceBar::hideBar() {
    hide();
    findMatches_.clear();
    currentMatchIndex_ = -1;
    emit matchesChanged({});
    editor_->setFocus();
}

// Re-scans the whole document for findInput_'s text, highlights every match, and
// jumps to whichever match is nearest the current cursor position.
void FindReplaceBar::runSearch() {
    QString needle = findInput_->text();
    findMatches_.clear();
    QList<QTextEdit::ExtraSelection> selections;

    if (!needle.isEmpty()) {
        QTextDocument* doc = editor_->document();
        QTextCursor cursor(doc);
        while (true) {
            cursor = doc->find(needle, cursor);
            if (cursor.isNull()) break;
            findMatches_.append(cursor.selectionStart());

            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(highlightColor_);
            sel.cursor = cursor;
            selections.append(sel);
        }
    }

    if (findMatches_.isEmpty()) {
        currentMatchIndex_ = -1;
    } else {
        int cursorPos = editor_->textCursor().selectionStart();
        currentMatchIndex_ = 0;
        for (int i = 0; i < findMatches_.size(); i++) {
            if (findMatches_[i] >= cursorPos) { currentMatchIndex_ = i; break; }
        }
        selectMatch(currentMatchIndex_);
    }

    updateStatusLabel();
    emit matchesChanged(selections);
}

void FindReplaceBar::selectMatch(int index) {
    if (index < 0 || index >= findMatches_.size()) return;
    QTextCursor cursor(editor_->document());
    cursor.setPosition(findMatches_[index]);
    cursor.setPosition(findMatches_[index] + findInput_->text().length(), QTextCursor::KeepAnchor);
    editor_->setTextCursor(cursor);
    editor_->centerCursor();
}

void FindReplaceBar::updateStatusLabel() {
    if (findMatches_.isEmpty())
        findStatusLabel_->setText(findInput_->text().isEmpty() ? "" : "No results");
    else
        findStatusLabel_->setText(QString("%1 of %2").arg(currentMatchIndex_ + 1).arg(findMatches_.size()));
}

void FindReplaceBar::onNext() {
    if (findMatches_.isEmpty()) return;
    currentMatchIndex_ = (currentMatchIndex_ + 1) % findMatches_.size();
    selectMatch(currentMatchIndex_);
    updateStatusLabel();
}

void FindReplaceBar::onPrev() {
    if (findMatches_.isEmpty()) return;
    currentMatchIndex_ = (currentMatchIndex_ - 1 + findMatches_.size()) % findMatches_.size();
    selectMatch(currentMatchIndex_);
    updateStatusLabel();
}

void FindReplaceBar::onReplaceClicked() {
    if (findMatches_.isEmpty() || currentMatchIndex_ < 0) return;

    QTextCursor cursor(editor_->document());
    cursor.setPosition(findMatches_[currentMatchIndex_]);
    cursor.setPosition(findMatches_[currentMatchIndex_] + findInput_->text().length(),
                        QTextCursor::KeepAnchor);
    cursor.insertText(replaceInput_->text());

    runSearch(); // positions shifted -- recompute and land on the next match
}

void FindReplaceBar::onReplaceAllClicked() {
    QString needle = findInput_->text();
    if (needle.isEmpty()) return;
    QString replacement = replaceInput_->text();

    QTextDocument* doc = editor_->document();
    QTextCursor editCursor(doc);
    editCursor.beginEditBlock();

    int count = 0;
    QTextCursor found = doc->find(needle);
    while (!found.isNull()) {
        found.insertText(replacement);
        count++;
        found = doc->find(needle, found.position());
    }
    editCursor.endEditBlock();

    runSearch();
    emit statusMessage(QString("Replaced %1 occurrence(s)").arg(count));
}

bool FindReplaceBar::eventFilter(QObject* obj, QEvent* event) {
    if ((obj == findInput_ || obj == replaceInput_) && event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape) {
            hideBar();
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (key->modifiers() & Qt::ShiftModifier) onPrev();
            else onNext();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
