#pragma once
#include <QString>
#include <QColor>
#include <QPalette>

// Applied directly via QTextEdit::ExtraSelection, not QSS, so raw QColor is needed here.
struct EditorHighlightColors {
    QColor error_bg;
    QColor warning_bg;
    QColor bracket_match_bg;
    QColor find_match_bg;
};

const EditorHighlightColors& editorHighlightColors(bool dark);

// Set once via qApp->setStyleSheet() so widgets created later (e.g. rebuilt serial monitors) pick it up too.
QString appStylesheet(bool dark);

// Must be set alongside the stylesheet -- QSS alone misses some native-rendered sub-elements (checkbox/table text).
QPalette appQPalette(bool dark);
