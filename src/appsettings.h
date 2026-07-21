#pragma once
#include <QSettings>
#include <QCoreApplication>

// The one settings.ini location, shared by the GUI and the headless CLI path.
inline QSettings appSettings() {
    return QSettings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);
}
