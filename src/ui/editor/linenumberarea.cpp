#include "src/ui/editor/linenumberarea.h"
#include "src/ui/editor/apireference.h"
#include <QKeyEvent>
#include <QTextCursor>
#include <QMenu>
#include <QRegularExpression>
#include <QLabel>
#include <QVBoxLayout>

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

    // Auto-close and skip-over-closer, checked ahead of the Key_BraceRight dedent handler
    // below so a skip-over "}" takes priority over that block's plain-text-insert fallback.
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
        duplicateCurrentLine();
        return;
    }

    if (pressed == commentToggleKey_) {
        toggleCommentSelection();
        return;
    }

    if (event->text() == ".") {
        QPlainTextEdit::keyPressEvent(event);
        emit dotTyped();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void EditorWithLines::duplicateCurrentLine() {
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

// Tries "Receiver.word" first (Serial.print, ...) since those share plain names with
// unrelated identifiers, then falls back to the bare word (pinMode, delay, ...).
void EditorWithLines::contextMenuEvent(QContextMenuEvent* event) {
    QMenu* menu = createStandardContextMenu();

    QTextCursor wordCursor = cursorForPosition(event->pos());
    wordCursor.select(QTextCursor::WordUnderCursor);
    QString word = wordCursor.selectedText();

    const ApiFunctionDoc* doc = nullptr;
    if (!word.isEmpty()) {
        QString textBeforeWord = wordCursor.block().text().left(
            wordCursor.selectionStart() - wordCursor.block().position());
        static const QRegularExpression receiverRe(R"((\w+)\.\s*$)");
        auto match = receiverRe.match(textBeforeWord);
        if (match.hasMatch())
            doc = lookupApiFunction(match.captured(1) + "." + word);
        if (!doc)
            doc = lookupApiFunction(word);
    }

    if (doc) {
        menu->addSeparator();
        QAction* action = menu->addAction(QString("API Reference: %1").arg(doc->signature));
        QPoint globalPos = event->globalPos();
        connect(action, &QAction::triggered, this, [this, globalPos, doc]() {
            QString html = QString("<b>%1</b><br>%2")
                .arg(doc->signature.toHtmlEscaped(), doc->summary.toHtmlEscaped());
            if (!doc->params.isEmpty()) {
                html += "<br><br><b>Params:</b><ul style='margin-left:-20px;'>";
                for (const QString& p : doc->params)
                    html += "<li>" + p.toHtmlEscaped() + "</li>";
                html += "</ul>";
            }
            if (!doc->returns.isEmpty())
                html += QString("<b>Returns:</b> %1").arg(doc->returns.toHtmlEscaped());

            // Qt::Popup, not QToolTip -- QToolTip's hover-tracking flashed and vanished
            // immediately when shown right after a QMenu closed; Qt::Popup is the same
            // window type QMenu uses, so it closes predictably on outside click/Escape.
            auto* popup = new QWidget(this, Qt::Popup);
            popup->setAttribute(Qt::WA_DeleteOnClose);
            popup->setStyleSheet(dark_
                ? "background:#2d2d30; color:#dcdcdc; border:1px solid #555555;"
                : "background:#f3f3f3; color:#1e1e1e; border:1px solid #b0b0b0;");
            auto* layout = new QVBoxLayout(popup);
            auto* label = new QLabel(html, popup);
            label->setTextFormat(Qt::RichText);
            label->setWordWrap(true);
            label->setMaximumWidth(360);
            layout->addWidget(label);
            popup->adjustSize();
            popup->move(globalPos);
            popup->show();
        });
    }

    menu->exec(event->globalPos());
    delete menu;
}