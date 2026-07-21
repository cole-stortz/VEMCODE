#include "src/ui/editor/keybindmanager.h"
#include "src/appsettings.h"
#include <QShortcut>

QKeySequence KeybindManager::load(QSettings& settings, const QString& id, QKeySequence def) {
    QString saved = settings.value("keybinds/" + id, QString()).toString();
    QKeySequence seq = saved.isEmpty() ? def : QKeySequence::fromString(saved);
    keybindSeq_[id] = seq;
    return seq;
}

void KeybindManager::registerShortcut(const QString& id, QShortcut* shortcut) {
    keybindShortcuts_[id] = shortcut;
}

void KeybindManager::apply(const QMap<QString, QKeySequence>& newBinds) {
    QSettings settings = appSettings();
    for (auto it = newBinds.constBegin(); it != newBinds.constEnd(); ++it) {
        keybindSeq_[it.key()] = it.value();
        settings.setValue("keybinds/" + it.key(), it.value().toString());
        auto shortcut = keybindShortcuts_.find(it.key());
        if (shortcut != keybindShortcuts_.end())
            shortcut.value()->setKey(it.value());
    }
}

QKeySequence KeybindManager::value(const QString& id) const {
    return keybindSeq_.value(id);
}
