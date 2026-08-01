#include "src/ui/editor/codehighlighter.h"

CodeHighlighter::CodeHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    buildRules(true);
}

void CodeHighlighter::setTheme(bool dark) {
    if (dark_ == dark) return;
    dark_ = dark;
    buildRules(dark);
    rehighlight();
}

void CodeHighlighter::buildRules(bool dark) {
    rules_.clear();
    HighlightRule rule;

    // Same hue families in both themes, darkened/desaturated for light mode
    // so text stays legible on a white-ish background instead of washing out.
    QColor preprocessor_color = dark ? QColor("#c586c0") : QColor("#9a3fa0");
    QColor keyword_color      = dark ? QColor("#569cd6") : QColor("#0451a5");
    QColor arduino_color      = dark ? QColor("#dcdcaa") : QColor("#7a5c00");
    QColor constant_color     = dark ? QColor("#4fc1ff") : QColor("#0e7490");
    QColor number_color       = dark ? QColor("#b5cea8") : QColor("#098658");
    QColor string_color       = dark ? QColor("#ce9178") : QColor("#a31515");
    QColor comment_color      = dark ? QColor("#6a9955") : QColor("#008000");

    QTextCharFormat preprocessor_fmt;
    preprocessor_fmt.setForeground(preprocessor_color);
    rule.pattern = QRegularExpression(R"(#\s*\w+)");
    rule.format  = preprocessor_fmt;
    rules_.append(rule);

    QTextCharFormat keyword_fmt;
    keyword_fmt.setForeground(keyword_color);
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
        "endif", "byte", "word", "boolean", "String",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "int8_t", "int16_t", "int32_t", "int64_t", "size_t",
        "PROGMEM"
    };
    for (const QString& kw : keywords) {
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(kw));
        rule.format = keyword_fmt;
        rules_.append(rule);
    }

    QTextCharFormat arduino_fmt;
    arduino_fmt.setForeground(arduino_color);
    const QStringList arduino_fns = {
        "pinMode", "digitalWrite", "digitalRead",
        "analogWrite", "analogRead",
        "delay", "delayMicroseconds", "millis", "micros",
        "setup", "loop",
        "tone", "noTone",
        "attachInterrupt", "detachInterrupt", "noInterrupts", "interrupts",
        "Serial", "Serial1", "Serial2", "SoftwareSerial",
        "begin", "print", "println", "available", "read", "write", "flush",
        "peek", "listen", "isListening", "overflow",
        "map", "constrain", "abs", "min", "max", "random", "randomSeed",
        "pulseIn", "shiftIn", "shiftOut",
        "Wire", "beginTransmission", "endTransmission", "requestFrom",
        "SPI", "SPISettings", "transfer", "beginTransaction", "endTransaction",
        "Servo", "attach", "detach", "attached",
        "EEPROM", "update",
        "LedControl", "shutdown", "setIntensity", "setLed", "setRow", "setColumn", "clearDisplay", "dim",
        "Adafruit_NeoPixel", "show", "setPixelColor", "setBrightness", "numPixels", "fill", "Color",
        "Adafruit_SSD1306", "Adafruit_GFX", "display", "drawPixel", "drawLine", "drawRect", "drawCircle",
        "fillRect", "fillCircle", "fillScreen", "setTextSize", "setTextColor", "setTextWrap",
        "invertDisplay", "drawBitmap", "drawChar",
        "LiquidCrystal", "createChar", "clear", "setCursor",
        "Keypad", "getKey", "makeKeymap",
        "DHT", "readTemperature", "readHumidity",
        "F", "digitalPinToInterrupt", "analogReference",
        "watch_variable", "ISR"
    };
    for (const QString& fn : arduino_fns) {
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(fn));
        rule.format = arduino_fmt;
        rules_.append(rule);
    }

    QTextCharFormat constant_fmt;
    constant_fmt.setForeground(constant_color);
    const QStringList constants = {
        "HIGH", "LOW", "INPUT", "OUTPUT", "INPUT_PULLUP",
        "LED_BUILTIN", "A0", "A1", "A2", "A3", "A4", "A5",
        "CHANGE", "RISING", "FALLING",
        "PI", "TWO_PI", "HALF_PI",
        "HEX", "DEC", "OCT", "BIN",
        "MSBFIRST", "LSBFIRST", "SPI_MODE0", "SPI_MODE1", "SPI_MODE2", "SPI_MODE3",
        "DDRB", "DDRC", "DDRD", "PORTB", "PORTC", "PORTD", "PINB", "PINC", "PIND",
        "PB0", "PB1", "PB2", "PB3", "PB4", "PB5", "PB6", "PB7",
        "PC0", "PC1", "PC2", "PC3", "PC4", "PC5", "PC6", "PC7",
        "PD0", "PD1", "PD2", "PD3", "PD4", "PD5", "PD6", "PD7",
        "TCCR1A", "TCCR1B", "TCNT1", "OCR1A", "OCR1B", "TIMSK1", "ICR1",
        "TCCR2A", "TCCR2B", "TCNT2", "OCR2A", "OCR2B", "TIMSK2",
        "CS10", "CS11", "CS12", "CS20", "CS21", "CS22",
        "WGM10", "WGM11", "WGM12", "WGM13", "WGM20", "WGM21", "WGM22",
        "COM1A0", "COM1A1", "COM1B0", "COM1B1", "COM2A0", "COM2A1", "COM2B0", "COM2B1",
        "TOIE1", "TOIE2", "OCIE1A", "OCIE1B", "OCIE2A", "OCIE2B",
        "TIMER1_OVF_vect", "TIMER1_COMPA_vect", "TIMER1_COMPB_vect",
        "TIMER2_OVF_vect", "TIMER2_COMPA_vect", "TIMER2_COMPB_vect",
        "true", "false", "null", "NULL"
    };
    for (const QString& c : constants) {
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(c));
        rule.format = constant_fmt;
        rules_.append(rule);
    }

    QTextCharFormat number_fmt;
    number_fmt.setForeground(number_color);
    rule.pattern = QRegularExpression(
        R"(\b(0x[0-9a-fA-F]+|\d+\.?\d*[fFuUlL]*)\b)");
    rule.format = number_fmt;
    rules_.append(rule);

    QTextCharFormat string_fmt;
    string_fmt.setForeground(string_color);
    string_pattern_ = QRegularExpression(R"("([^"\\]|\\.)*")");
    rule.pattern = string_pattern_;
    rule.format  = string_fmt;
    rules_.append(rule);

    sl_comment_format_.setForeground(comment_color);
    sl_comment_format_.setFontItalic(true);
    sl_comment_start_ = QRegularExpression(R"(//)");

    comment_format_.setForeground(comment_color);
    comment_format_.setFontItalic(true);
    comment_start_ = QRegularExpression(R"(/\*)");
    comment_end_   = QRegularExpression(R"(\*/)");
}

bool CodeHighlighter::insideString(const QVector<std::pair<int, int>>& stringRanges, int start) const {
    for (const auto& [rangeStart, rangeEnd] : stringRanges)
        if (start >= rangeStart && start < rangeEnd) return true;
    return false;
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

    QVector<std::pair<int, int>> stringRanges;
    {
        QRegularExpressionMatchIterator it = string_pattern_.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            stringRanges.append({m.capturedStart(), m.capturedStart() + m.capturedLength()});
        }
    }

    // Single-line comment -- find the first "//" that isn't part of a string
    // literal (e.g. a "http://..." URL) and color it through end of line.
    {
        QRegularExpressionMatchIterator it = sl_comment_start_.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (!insideString(stringRanges, m.capturedStart())) {
                setFormat(m.capturedStart(), text.length() - m.capturedStart(), sl_comment_format_);
                break;
            }
        }
    }

    setCurrentBlockState(0);

    int start = 0;
    if (previousBlockState() != 1) {
        start = text.indexOf(comment_start_);
        while (start >= 0 && insideString(stringRanges, start))
            start = text.indexOf(comment_start_, start + 1);
    }

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
        while (start >= 0 && insideString(stringRanges, start))
            start = text.indexOf(comment_start_, start + 1);
    }
}
