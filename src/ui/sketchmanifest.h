#pragma once
#include <QString>
#include <QList>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// One entry in templates/manifest.json or examples/manifest.json. `category`
// is empty for templates -- only the example library groups by component type.
struct SketchManifestEntry {
    QString name;
    QString folder;
    QString file;
    QString category;
    QString description;
};

// Reads a manifest.json (an array of {name, folder, file, category?, description})
// sitting next to a set of "<folder>/<file>" sketch directories. Returns an empty
// list if the manifest is missing or malformed, callers should treat that as
// "nothing to offer" rather than an error.
inline QList<SketchManifestEntry> loadSketchManifest(const QString& manifestPath) {
    QList<SketchManifestEntry> entries;

    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return entries;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return entries;

    for (const QJsonValue& v : doc.array()) {
        QJsonObject obj = v.toObject();
        SketchManifestEntry entry;
        entry.name        = obj.value("name").toString();
        entry.folder      = obj.value("folder").toString();
        entry.file        = obj.value("file").toString();
        entry.category    = obj.value("category").toString();
        entry.description = obj.value("description").toString();
        if (entry.name.isEmpty() || entry.folder.isEmpty() || entry.file.isEmpty()) continue;
        entries.push_back(entry);
    }
    return entries;
}
