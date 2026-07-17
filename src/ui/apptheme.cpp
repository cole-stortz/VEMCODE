#include "src/ui/apptheme.h"

namespace {

struct AppPalette {
    QColor bg_chrome;        // toolbar, editor, tables, line edits, tab pane
    QColor bg_panel_header;  // panel headers, inactive tabs, table headers, port labels
    QColor border;
    QColor border_light;     // button/input outlines

    QColor text_primary;     // editor/table text, generic label fallback
    QColor text_title;       // toolbar title, selected tab
    QColor text_muted;       // panel headers, board label
    QColor text_dim;         // port labels, slider handle

    QColor btn_run, btn_run_hover;
    QColor btn_stop, btn_stop_hover;
    QColor btn_outline_text, btn_outline_hover_bg, btn_outline_hover_text;
    QColor btn_disabled_bg, btn_disabled_text;
    QColor toggle_checked_bg, toggle_checked_border, toggle_checked_text;

    QColor serial_text;
};

const AppPalette kDark{
    QColor("#1e1e1e"), QColor("#252526"), QColor("#333333"), QColor("#444444"),
    QColor("#d4d4d4"), QColor("#cccccc"), QColor("#888888"), QColor("#666666"),
    QColor("#2d7a2d"), QColor("#3a9a3a"),
    QColor("#7a2d2d"), QColor("#9a3a3a"),
    QColor("#aaaaaa"), QColor("#2a2a2a"), QColor("#ffffff"),
    QColor("#2a2a2a"), QColor("#555555"),
    QColor("#3a5a8c"), QColor("#4a7abc"), QColor("#ffffff"),
    QColor("#4ec94e"),
};

const AppPalette kLight{
    QColor("#f5f5f7"), QColor("#e8e8ec"), QColor("#d0d0d5"), QColor("#c0c0c8"),
    QColor("#1e1e1e"), QColor("#2a2a2a"), QColor("#6a6a72"), QColor("#8a8a92"),
    QColor("#2d9d2d"), QColor("#38b038"),
    QColor("#c23b3b"), QColor("#d94a4a"),
    QColor("#55555f"), QColor("#dcdce2"), QColor("#000000"),
    QColor("#dcdce2"), QColor("#aaaaaa"),
    QColor("#b8d0f0"), QColor("#6a90c8"), QColor("#1a1a2a"),
    QColor("#1a7a1a"),
};

const EditorHighlightColors kDarkHighlights{
    QColor("#3a0000"), QColor("#3a3400"), QColor("#264f78"), QColor("#5a4a00"),
};

const EditorHighlightColors kLightHighlights{
    QColor("#ffd6d6"), QColor("#fff3b0"), QColor("#b8d4f0"), QColor("#ffe0b0"),
};

} // namespace

const EditorHighlightColors& editorHighlightColors(bool dark) {
    return dark ? kDarkHighlights : kLightHighlights;
}

QPalette appQPalette(bool dark) {
    const AppPalette& p = dark ? kDark : kLight;

    QPalette pal;
    pal.setColor(QPalette::Window,          p.bg_chrome);
    pal.setColor(QPalette::WindowText,      p.text_primary);
    pal.setColor(QPalette::Base,            p.bg_chrome);
    pal.setColor(QPalette::AlternateBase,   p.bg_panel_header);
    pal.setColor(QPalette::Text,            p.text_primary);
    pal.setColor(QPalette::Button,          p.bg_panel_header);
    pal.setColor(QPalette::ButtonText,      p.text_primary);
    pal.setColor(QPalette::ToolTipBase,     p.bg_chrome);
    pal.setColor(QPalette::ToolTipText,     p.text_primary);
    pal.setColor(QPalette::PlaceholderText, p.text_muted);
    pal.setColor(QPalette::Highlight,       p.toggle_checked_bg);
    pal.setColor(QPalette::HighlightedText, p.toggle_checked_text);

    pal.setColor(QPalette::Disabled, QPalette::WindowText, p.btn_disabled_text);
    pal.setColor(QPalette::Disabled, QPalette::Text,       p.btn_disabled_text);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, p.btn_disabled_text);

    return pal;
}

QString appStylesheet(bool dark) {
    const AppPalette& p = dark ? kDark : kLight;

    // Placeholder order below is 1..21, in the exact order the .arg() chain
    // supplies them -- QString::arg() fills the lowest-numbered unfilled
    // placeholder on each call, so any gap in the numbering shifts every
    // later substitution. Keep this list and the chain below in lockstep.
    QString qss =
        "QMainWindow, QDialog, QMessageBox { background: %1; color: %5; }"
        "QLabel { color: %5; }"
        "QCheckBox { color: %5; }"
        "QComboBox { background: %1; color: %5; border: 1px solid %4; border-radius: 3px; padding: 2px 6px; }"

        "QWidget#toolbar { background: %1; border-bottom: 1px solid %3; }"
        "QLabel#appTitle { color: %6; font-size: 13px; font-weight: bold; border: none; background: transparent; }"
        "QLabel#boardLabel { color: %8; font-size: 11px; border: none; background: transparent; }"
        "QLabel[role=\"muted-label\"] { color: %7; font-size: 11px; border: none; background: transparent; }"

        "QWidget[role=\"panel-header\"], QLabel[role=\"panel-header\"] {"
        "  background: %2; color: %7; font-size: 10px; font-weight: bold; border-bottom: 1px solid %3; }"
        "QWidget[role=\"input-row\"] { background: %2; border-top: 1px solid %3; }"
        "QLabel[role=\"port-label\"] { background: %2; color: %8; font-size: 10px; padding-left: 4px; border-bottom: 1px solid %3; }"
        "QLabel[role=\"port-label-bordered\"] { background: %2; color: %8; font-size: 10px; padding-left: 4px;"
        "  border-left: 1px solid %3; border-bottom: 1px solid %3; }"

        "QPushButton#btnRun { background: %9; color: #ffffff; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }"
        "QPushButton#btnRun:hover { background: %10; }"
        "QPushButton#btnRun:disabled { background: %16; color: %17; }"
        "QPushButton#btnStop { background: %11; color: #ffffff; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }"
        "QPushButton#btnStop:hover { background: %12; }"
        "QPushButton#btnStop:disabled { background: %16; color: %17; }"

        "QPushButton[role=\"outline\"] { background: transparent; color: %13; border: 1px solid %4; border-radius: 4px; font-size: 12px; padding: 0 10px; }"
        "QPushButton[role=\"outline\"]:hover { background: %14; color: %15; }"

        "QPushButton[role=\"toggle\"] { background: transparent; color: %13; border: 1px solid %4; border-radius: 4px; font-size: 11px; padding: 0 8px; }"
        "QPushButton[role=\"toggle\"]:hover { background: %14; color: %15; }"
        "QPushButton[role=\"toggle\"]:checked { background: %18; color: %20; border-color: %19; }"

        "QSplitter::handle { background: %3; }"

        "QPlainTextEdit#codeEditor { background: %1; color: %5; border: none; font-family: 'Courier New', monospace; }"
        "QPlainTextEdit[role=\"serial\"] { background: %1; color: %21; border: none; font-family: 'Courier New', monospace; font-size: 12px; }"

        "QLineEdit { background: %1; color: %5; border: 1px solid %4; border-radius: 3px; padding: 3px 6px; font-size: 12px; }"

        "QSlider::groove:horizontal { background: %3; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %8; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }"
        "QSlider::handle:horizontal:hover { background: %6; }"

        "QTabWidget { background: %2; }"
        "QTabWidget::pane { border: none; background: %1; }"
        "QTabWidget::tab-bar { background: %2; }"
        "QTabBar { background: %2; }"
        "QTabBar::tab { background: %2; color: %7; padding: 4px 14px; font-size: 11px; border: none; border-right: 1px solid %3; }"
        "QTabBar::tab:selected { background: %1; color: %6; }"
        "QTabBar::tab:hover { background: %14; color: %8; }"

        "QTableWidget { background: %1; color: %5; border: none; font-family: 'Courier New'; font-size: 12px; }"
        "QHeaderView::section { background: %2; color: %7; border: none; padding: 4px; }";

    return qss
        .arg(p.bg_chrome.name())            // %1
        .arg(p.bg_panel_header.name())      // %2
        .arg(p.border.name())               // %3
        .arg(p.border_light.name())         // %4
        .arg(p.text_primary.name())         // %5
        .arg(p.text_title.name())           // %6
        .arg(p.text_muted.name())           // %7
        .arg(p.text_dim.name())             // %8
        .arg(p.btn_run.name())              // %9
        .arg(p.btn_run_hover.name())        // %10
        .arg(p.btn_stop.name())             // %11
        .arg(p.btn_stop_hover.name())       // %12
        .arg(p.btn_outline_text.name())     // %13
        .arg(p.btn_outline_hover_bg.name()) // %14
        .arg(p.btn_outline_hover_text.name())// %15
        .arg(p.btn_disabled_bg.name())      // %16
        .arg(p.btn_disabled_text.name())    // %17
        .arg(p.toggle_checked_bg.name())    // %18
        .arg(p.toggle_checked_border.name())// %19
        .arg(p.toggle_checked_text.name())  // %20
        .arg(p.serial_text.name());         // %21
}
