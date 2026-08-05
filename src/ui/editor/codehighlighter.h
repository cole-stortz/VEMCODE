#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>
#include <utility>

// Arduino C++ syntax highlighter for QPlainTextEdit.
class CodeHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit CodeHighlighter(QTextDocument* parent = nullptr);

    void setTheme(bool dark);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };

    void buildRules(bool dark);

    QVector<HighlightRule> rules_;

    // Matched separately from rules_ so comment detection can skip a // or /* */ that's actually inside a string.
    QRegularExpression string_pattern_;

    // Single-line comment state
    QTextCharFormat    sl_comment_format_;
    QRegularExpression sl_comment_start_;

    // Multi-line comment state
    QTextCharFormat  comment_format_;
    QRegularExpression comment_start_;
    QRegularExpression comment_end_;

    // True if `start` falls inside any string literal match on this line.
    bool insideString(const QVector<std::pair<int, int>>& stringRanges, int start) const;

    bool dark_ = true;
};