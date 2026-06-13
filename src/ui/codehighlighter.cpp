#include "src/ui/codehighlighter.h"

CodeHighlighter::CodeHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    HighlightRule rule;

    QTextCharFormat preprocessor_fmt;
    preprocessor_fmt.setForeground(QColor("#c586c0"));
    rule.pattern = QRegularExpression(R"(#\s*\w+)");
    rule.format  = preprocessor_fmt;
    rules_.append(rule);

    QTextCharFormat keyword_fmt;
    keyword_fmt.setForeground(QColor("#569cd6"));
    keyword_fmt.setFontWeight(QFont::Bold);
    const QStringList keywords = {
        "void", "int", "float", "double", "bool", "char",
        "long", "short", "unsigned", "signed", "const",
        "static", "extern", "volatile", "return", "if",
        "else", "for", "while", "do", "switch", "case",
        "break", "continue", "class", "struct", "enum",
        "true", "false", "nullptr", "new", "delete",
        "public", "private", "protected", "namespace",
        "using", "include", "define", "ifdef", "ifndef",
        "endif", "byte", "word", "boolean", "String"
    };
    for (const QString& kw : keywords) {
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(kw));
        rule.format = keyword_fmt;
        rules_.append(rule);
    }

    QTextCharFormat arduino_fmt;
    arduino_fmt.setForeground(QColor("#dcdcaa"));
    const QStringList arduino_fns = {
        "pinMode", "digitalWrite", "digitalRead",
        "analogWrite", "analogRead",
        "delay", "delayMicroseconds", "millis", "micros",
        "setup", "loop",
        "tone", "noTone",
        "attachInterrupt", "detachInterrupt",
        "Serial", "begin", "print", "println", "available", "read",
        "map", "constrain", "abs", "min", "max", "random", "randomSeed",
        "pulseIn", "shiftIn", "shiftOut",
        "watch_variable"
    };
    for (const QString& fn : arduino_fns) {
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(fn));
        rule.format = arduino_fmt;
        rules_.append(rule);
    }

    QTextCharFormat constant_fmt;
    constant_fmt.setForeground(QColor("#4fc1ff"));
    const QStringList constants = {
        "HIGH", "LOW", "INPUT", "OUTPUT", "INPUT_PULLUP",
        "LED_BUILTIN", "A0", "A1", "A2", "A3", "A4", "A5",
        "CHANGE", "RISING", "FALLING",
        "PI", "TWO_PI", "HALF_PI",
        "true", "false", "null", "NULL"
    };
    for (const QString& c : constants) {
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(c));
        rule.format = constant_fmt;
        rules_.append(rule);
    }

    QTextCharFormat number_fmt;
    number_fmt.setForeground(QColor("#b5cea8"));
    rule.pattern = QRegularExpression(
        R"(\b(0x[0-9a-fA-F]+|\d+\.?\d*[fFuUlL]*)\b)");
    rule.format = number_fmt;
    rules_.append(rule);

    QTextCharFormat string_fmt;
    string_fmt.setForeground(QColor("#ce9178"));
    rule.pattern = QRegularExpression(R"("([^"\\]|\\.)*")");
    rule.format  = string_fmt;
    rules_.append(rule);

    QTextCharFormat sl_comment_fmt;
    sl_comment_fmt.setForeground(QColor("#6a9955"));
    sl_comment_fmt.setFontItalic(true);
    rule.pattern = QRegularExpression(R"(//[^\n]*)");
    rule.format  = sl_comment_fmt;
    rules_.append(rule);

    comment_format_.setForeground(QColor("#6a9955"));
    comment_format_.setFontItalic(true);
    comment_start_ = QRegularExpression(R"(/\*)");
    comment_end_   = QRegularExpression(R"(\*/)");
}

void CodeHighlighter::highlightBlock(const QString& text) {
    for (const HighlightRule& rule : rules_) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(),
                      rule.format);
        }
    }

    setCurrentBlockState(0);

    int start = 0;
    if (previousBlockState() != 1)
        start = text.indexOf(comment_start_);

    while (start >= 0) {
        QRegularExpressionMatch end_match =
            comment_end_.match(text, start);

        int end_index  = end_match.capturedStart();
        int comment_len;

        if (end_index == -1) {
            // Comment continues to next block
            setCurrentBlockState(1);
            comment_len = text.length() - start;
        } else {
            comment_len = end_index - start + end_match.capturedLength();
        }

        setFormat(start, comment_len, comment_format_);
        start = text.indexOf(comment_start_, start + comment_len);
    }
}