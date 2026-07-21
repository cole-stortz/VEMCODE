#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>
#include <utility>

// Arduino C++ syntax highlighter for QPlainTextEdit.
// Highlights: keywords, types, Arduino functions, strings,
// comments, numbers, preprocessor directives, and constants.
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

    // String literals are matched separately (not through rules_) so the
    // comment handling below can check "is this position inside a string"
    // before coloring a // or /* */ that's actually just part of a string's
    // text (e.g. a "http://..." URL).
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