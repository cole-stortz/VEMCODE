#pragma once
#include <QMap>
#include <QString>
#include <QKeySequence>
#include <QSettings>

class QShortcut;

// Owns the current sequence per keybind id (source of truth for both
// QShortcut-backed actions and the raw key comparisons in
// MainWindow::eventFilter) and its persistence to settings.ini. Shortcut
// construction/wiring stays in MainWindow -- each one pairs with its own
// callback, so there's nothing generic to factor out there.
class KeybindManager {
public:
    // Reads "keybinds/<id>" from settings (falling back to `def`), remembers
    // it, and returns it -- meant to seed a QShortcut's initial sequence.
    QKeySequence load(QSettings& settings, const QString& id, QKeySequence def);

    // Associates a live QShortcut with `id` so apply() can rebind it later.
    void registerShortcut(const QString& id, QShortcut* shortcut);

    // Persists every id -> sequence and rebinds any live QShortcut --
    // no restart needed to pick up a remap.
    void apply(const QMap<QString, QKeySequence>& newBinds);

    QKeySequence value(const QString& id) const;
    const QMap<QString, QKeySequence>& all() const { return keybindSeq_; }

private:
    QMap<QString, QKeySequence> keybindSeq_;
    QMap<QString, QShortcut*>   keybindShortcuts_;
};
