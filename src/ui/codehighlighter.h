#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

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

    // Multi-line comment state
    QTextCharFormat  comment_format_;
    QRegularExpression comment_start_;
    QRegularExpression comment_end_;

    bool dark_ = true;
};