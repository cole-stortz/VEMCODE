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

const AppPalette kDark = [] {
    AppPalette p;
    p.bg_chrome              = QColor("#1e1e1e");
    p.bg_panel_header        = QColor("#252526");
    p.border                 = QColor("#333333");
    p.border_light           = QColor("#444444");
    p.text_primary           = QColor("#d4d4d4");
    p.text_title             = QColor("#cccccc");
    p.text_muted             = QColor("#888888");
    p.text_dim               = QColor("#666666");
    p.btn_run                = QColor("#2d7a2d");
    p.btn_run_hover          = QColor("#3a9a3a");
    p.btn_stop               = QColor("#7a2d2d");
    p.btn_stop_hover         = QColor("#9a3a3a");
    p.btn_outline_text       = QColor("#aaaaaa");
    p.btn_outline_hover_bg   = QColor("#2a2a2a");
    p.btn_outline_hover_text = QColor("#ffffff");
    p.btn_disabled_bg        = QColor("#2a2a2a");
    p.btn_disabled_text      = QColor("#555555");
    p.toggle_checked_bg      = QColor("#3a5a8c");
    p.toggle_checked_border  = QColor("#4a7abc");
    p.toggle_checked_text    = QColor("#ffffff");
    p.serial_text            = QColor("#4ec94e");
    return p;
}();

const AppPalette kLight = [] {
    AppPalette p;
    p.bg_chrome              = QColor("#f5f5f7");
    p.bg_panel_header        = QColor("#e8e8ec");
    p.border                 = QColor("#d0d0d5");
    p.border_light           = QColor("#c0c0c8");
    p.text_primary           = QColor("#1e1e1e");
    p.text_title             = QColor("#2a2a2a");
    p.text_muted             = QColor("#6a6a72");
    p.text_dim               = QColor("#8a8a92");
    p.btn_run                = QColor("#2d9d2d");
    p.btn_run_hover          = QColor("#38b038");
    p.btn_stop               = QColor("#c23b3b");
    p.btn_stop_hover         = QColor("#d94a4a");
    p.btn_outline_text       = QColor("#55555f");
    p.btn_outline_hover_bg   = QColor("#dcdce2");
    p.btn_outline_hover_text = QColor("#000000");
    p.btn_disabled_bg        = QColor("#dcdce2");
    p.btn_disabled_text      = QColor("#aaaaaa");
    p.toggle_checked_bg      = QColor("#b8d0f0");
    p.toggle_checked_border  = QColor("#6a90c8");
    p.toggle_checked_text    = QColor("#1a1a2a");
    p.serial_text            = QColor("#1a7a1a");
    return p;
}();

// Substitutes %{field_name} tokens in a stylesheet template with the matching
// AppPalette color. Field names are looked up by name, not position, so
// adding/reordering AppPalette members can't silently shift a color onto the
// wrong selector -- a token left unmatched shows up as literal "%{...}" text
// in the rendered UI instead of a swapped color.
QString substitutePalette(QString qss, const AppPalette& p) {
    struct Token { const char* name; QColor color; };
    const Token tokens[] = {
        {"bg_chrome", p.bg_chrome},
        {"bg_panel_header", p.bg_panel_header},
        {"border", p.border},
        {"border_light", p.border_light},
        {"text_primary", p.text_primary},
        {"text_title", p.text_title},
        {"text_muted", p.text_muted},
        {"text_dim", p.text_dim},
        {"btn_run", p.btn_run},
        {"btn_run_hover", p.btn_run_hover},
        {"btn_stop", p.btn_stop},
        {"btn_stop_hover", p.btn_stop_hover},
        {"btn_outline_text", p.btn_outline_text},
        {"btn_outline_hover_bg", p.btn_outline_hover_bg},
        {"btn_outline_hover_text", p.btn_outline_hover_text},
        {"btn_disabled_bg", p.btn_disabled_bg},
        {"btn_disabled_text", p.btn_disabled_text},
        {"toggle_checked_bg", p.toggle_checked_bg},
        {"toggle_checked_border", p.toggle_checked_border},
        {"toggle_checked_text", p.toggle_checked_text},
        {"serial_text", p.serial_text},
    };
    for (const auto& t : tokens)
        qss.replace(QString("%{%1}").arg(t.name), t.color.name());
    return qss;
}

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

    QString qss =
        "QMainWindow, QDialog, QMessageBox { background: %{bg_chrome}; color: %{text_primary}; }"
        "QLabel { color: %{text_primary}; }"
        "QCheckBox { color: %{text_primary}; }"
        "QComboBox { background: %{bg_chrome}; color: %{text_primary}; border: 1px solid %{border_light}; border-radius: 3px; padding: 2px 6px; }"

        "QWidget#toolbar { background: %{bg_chrome}; border-bottom: 1px solid %{border}; }"
        "QLabel#appTitle { color: %{text_title}; font-size: 13px; font-weight: bold; border: none; background: transparent; }"
        "QLabel#boardLabel { color: %{text_dim}; font-size: 11px; border: none; background: transparent; }"
        "QLabel[role=\"muted-label\"] { color: %{text_muted}; font-size: 11px; border: none; background: transparent; }"

        "QWidget[role=\"panel-header\"], QLabel[role=\"panel-header\"] {"
        "  background: %{bg_panel_header}; color: %{text_muted}; font-size: 10px; font-weight: bold; border-bottom: 1px solid %{border}; }"
        "QWidget[role=\"input-row\"] { background: %{bg_panel_header}; border-top: 1px solid %{border}; }"
        "QLabel[role=\"port-label\"] { background: %{bg_panel_header}; color: %{text_dim}; font-size: 10px; padding-left: 4px; border-bottom: 1px solid %{border}; }"
        "QLabel[role=\"port-label-bordered\"] { background: %{bg_panel_header}; color: %{text_dim}; font-size: 10px; padding-left: 4px;"
        "  border-left: 1px solid %{border}; border-bottom: 1px solid %{border}; }"

        "QPushButton#btnRun { background: %{btn_run}; color: #ffffff; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }"
        "QPushButton#btnRun:hover { background: %{btn_run_hover}; }"
        "QPushButton#btnRun:disabled { background: %{btn_disabled_bg}; color: %{btn_disabled_text}; }"
        "QPushButton#btnStop { background: %{btn_stop}; color: #ffffff; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }"
        "QPushButton#btnStop:hover { background: %{btn_stop_hover}; }"
        "QPushButton#btnStop:disabled { background: %{btn_disabled_bg}; color: %{btn_disabled_text}; }"

        "QPushButton[role=\"outline\"] { background: transparent; color: %{btn_outline_text}; border: 1px solid %{border_light}; border-radius: 4px; font-size: 12px; padding: 0 10px; }"
        "QPushButton[role=\"outline\"]:hover { background: %{btn_outline_hover_bg}; color: %{btn_outline_hover_text}; }"

        "QPushButton[role=\"toggle\"] { background: transparent; color: %{btn_outline_text}; border: 1px solid %{border_light}; border-radius: 4px; font-size: 11px; padding: 0 8px; }"
        "QPushButton[role=\"toggle\"]:hover { background: %{btn_outline_hover_bg}; color: %{btn_outline_hover_text}; }"
        "QPushButton[role=\"toggle\"]:checked { background: %{toggle_checked_bg}; color: %{toggle_checked_text}; border-color: %{toggle_checked_border}; }"

        "QSplitter::handle { background: %{border}; }"

        "QPlainTextEdit#codeEditor { background: %{bg_chrome}; color: %{text_primary}; border: none; font-family: 'Courier New', monospace; }"
        "QPlainTextEdit[role=\"serial\"] { background: %{bg_chrome}; color: %{serial_text}; border: none; font-family: 'Courier New', monospace; font-size: 12px; }"

        "QLineEdit { background: %{bg_chrome}; color: %{text_primary}; border: 1px solid %{border_light}; border-radius: 3px; padding: 3px 6px; font-size: 12px; }"

        "QSlider::groove:horizontal { background: %{border}; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %{text_dim}; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }"
        "QSlider::handle:horizontal:hover { background: %{text_title}; }"

        "QTabWidget { background: %{bg_panel_header}; }"
        "QTabWidget::pane { border: none; background: %{bg_chrome}; }"
        "QTabWidget::tab-bar { background: %{bg_panel_header}; }"
        "QTabBar { background: %{bg_panel_header}; }"
        "QTabBar::tab { background: %{bg_panel_header}; color: %{text_muted}; padding: 4px 14px; font-size: 11px; border: none; border-right: 1px solid %{border}; }"
        "QTabBar::tab:selected { background: %{bg_chrome}; color: %{text_title}; }"
        "QTabBar::tab:hover { background: %{btn_outline_hover_bg}; color: %{text_dim}; }"

        "QTableWidget { background: %{bg_chrome}; color: %{text_primary}; border: none; font-family: 'Courier New'; font-size: 12px; }"
        "QHeaderView::section { background: %{bg_panel_header}; color: %{text_muted}; border: none; padding: 4px; }";

    return substitutePalette(qss, p);
}
