#include "src/ui/editor/linenumberarea.h"
#include <QKeyEvent>
#include <QTextCursor>

void EditorWithLines::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Tab) {
        insertPlainText("    ");
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QTextCursor cursor = textCursor();

        cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        int spaces = 0;
        for (QChar c : line) {
            if (c == ' ') spaces++;
            else break;
        }

        QString trimmed = line.trimmed();
        if (trimmed.endsWith('{'))
            spaces += 4;

        cursor = textCursor();
        cursor.insertText("\n" + QString(spaces, ' '));
        setTextCursor(cursor);
        return;
    }

    // Auto-close (, [, {, " and skip over an already-present closer instead of
    // doubling it. Checked ahead of the Key_BraceRight dedent handler below so a
    // skip-over "}" takes priority over that block's plain-text-insert fallback.
    {
        QString typedText = event->text();
        if (typedText.size() == 1) {
            QChar tc = typedText.at(0);
            static const QString kOpen  = "([{\"";
            static const QString kClose = ")]}\"";

            if (kClose.contains(tc)) {
                QTextCursor cursor = textCursor();
                if (!cursor.hasSelection() &&
                    document()->characterAt(cursor.position()) == tc) {
                    cursor.movePosition(QTextCursor::NextCharacter);
                    setTextCursor(cursor);
                    return;
                }
            }
            if (kOpen.contains(tc) && !textCursor().hasSelection()) {
                QChar closer = kClose.at(kOpen.indexOf(tc));
                insertPlainText(QString(tc) + QString(closer));
                QTextCursor cursor = textCursor();
                cursor.movePosition(QTextCursor::PreviousCharacter);
                setTextCursor(cursor);
                return;
            }
        }
    }

    if (event->key() == Qt::Key_BraceRight) {
        QTextCursor cursor = textCursor();

        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        if (line.trimmed().isEmpty() && line.length() > 0) {
            int new_indent = qMax(0, (int)line.length() - 4);
            cursor.removeSelectedText();
            cursor.insertText(QString(new_indent, ' ') + "}");
            setTextCursor(cursor);
            return;
        }
    }

    QKeySequence pressed(event->keyCombination());

    if (pressed == completionKey_) {
        emit completionRequested();
        return;
    }

    if (pressed == duplicateLineKey_) {
        QTextCursor cursor = textCursor();
        int col = cursor.positionInBlock();

        QTextCursor lineCursor = cursor;
        lineCursor.movePosition(QTextCursor::StartOfLine);
        lineCursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString line = lineCursor.selectedText();

        cursor.movePosition(QTextCursor::EndOfLine);
        cursor.insertText("\n" + line);
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, col);
        setTextCursor(cursor);
        return;
    }

    if (pressed == commentToggleKey_) {
        toggleCommentSelection();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void EditorWithLines::toggleCommentSelection() {
    QTextCursor cursor = textCursor();
    QTextBlock startBlock = document()->findBlock(cursor.selectionStart());
    QTextBlock endBlock   = document()->findBlock(cursor.selectionEnd());
    if (endBlock != startBlock && cursor.selectionEnd() == endBlock.position())
        endBlock = endBlock.previous();

    bool allCommented = true;
    for (QTextBlock b = startBlock; b.isValid(); b = b.next()) {
        QString trimmed = b.text().trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith("//")) allCommented = false;
        if (b == endBlock) break;
    }

    cursor.beginEditBlock();
    for (QTextBlock b = startBlock; b.isValid(); b = b.next()) {
        QString text = b.text();
        int firstNonSpace = 0;
        while (firstNonSpace < text.length() && text[firstNonSpace] == ' ') firstNonSpace++;

        QTextCursor lineCursor(b);
        if (allCommented) {
            if (text.mid(firstNonSpace, 2) == "//") {
                int len = (text.mid(firstNonSpace, 3) == "// ") ? 3 : 2;
                lineCursor.setPosition(b.position() + firstNonSpace);
                lineCursor.setPosition(b.position() + firstNonSpace + len, QTextCursor::KeepAnchor);
                lineCursor.removeSelectedText();
            }
        } else if (!text.trimmed().isEmpty()) {
            lineCursor.setPosition(b.position() + firstNonSpace);
            lineCursor.insertText("// ");
        }
        if (b == endBlock) break;
    }
    cursor.endEditBlock();
}