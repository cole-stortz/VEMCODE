#pragma once
#include <QMap>
#include <QString>
#include <QKeySequence>
#include <QSettings>

class QShortcut;

// Source of truth for keybind sequences (QShortcut-backed actions and the raw key
// comparisons in MainWindow::eventFilter) and their persistence to settings.ini.
class KeybindManager {
public:
    // Reads "keybinds/<id>" from settings, falling back to `def`; seeds a QShortcut's initial sequence.
    QKeySequence load(QSettings& settings, const QString& id, QKeySequence def);

    // Associates a live QShortcut with `id` so apply() can rebind it later.
    void registerShortcut(const QString& id, QShortcut* shortcut);

    // Persists every id -> sequence and rebinds any live QShortcut; no restart needed.
    void apply(const QMap<QString, QKeySequence>& newBinds);

    QKeySequence value(const QString& id) const;
    const QMap<QString, QKeySequence>& all() const { return keybindSeq_; }

private:
    QMap<QString, QKeySequence> keybindSeq_;
    QMap<QString, QShortcut*>   keybindShortcuts_;
};
