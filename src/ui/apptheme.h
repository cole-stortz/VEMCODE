#pragma once
#include <QString>
#include <QColor>
#include <QPalette>

// Editor-highlight background colors (compile error/warning lines, matched
// bracket, find match) -- not QSS, applied directly via QTextEdit::ExtraSelection,
// so MainWindow needs the raw QColor rather than a stylesheet string.
struct EditorHighlightColors {
    QColor error_bg;
    QColor warning_bg;
    QColor bracket_match_bg;
    QColor find_match_bg;
};

const EditorHighlightColors& editorHighlightColors(bool dark);

// Full QApplication-wide stylesheet covering the toolbar, panels, buttons,
// tables, line edits, tabs, and splitters -- set once via qApp->setStyleSheet()
// so every widget (including ones created later, e.g. rebuilt serial monitors)
// picks it up automatically via class/property selectors.
QString appStylesheet(bool dark);

// Companion QPalette, set via qApp->setPalette() alongside the stylesheet.
// QSS "color"/"background" alone doesn't reliably reach every native-rendered
// sub-element (checkbox label text, table item text, tab-bar fill in unstyled
// gaps) -- those fall back to the application palette, so both need to change
// together or some widgets stay stuck looking like the old theme.
QPalette appQPalette(bool dark);
