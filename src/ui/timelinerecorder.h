#pragma once
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <QPointF>
#include <vector>
#include "src/core/circuit/circuitdetector.h"
#include "src/core/circuit/componentitem.h"

// Records canvas interactions and Serial sends as `.timeline` lines for headless replay.
// Purely a writer -- see src/core/host/timeline.h (format) and TestRunner::dispatchAction (verb shapes) for the reader.
class TimelineRecorder {
public:
    void clear();
    void setActive(bool active) { active_ = active; }
    bool isActive() const { return active_; }
    bool isEmpty() const { return lines_.isEmpty(); }

    // Keypad/DHT have no matching verb in TestRunner::dispatchAction -- silently skipped.
    void recordComponentInput(int pin, ComponentEventType type, const QVariant& value,
                               double time_s, const std::vector<DetectedComponent>& components);
    void recordSerialSend(const QString& text, double time_s);

    bool exportToFile(const QString& path) const;

private:
    const DetectedComponent* findComponentForPin(int pin,
        const std::vector<DetectedComponent>& components) const;
    void appendLine(double time_s, const QString& verb, const QString& target,
                     const QStringList& args = {});

    // Per-joystick last-known axis value (keyed by VRX pin) -- format has no per-axis SET, only full MOVE <x> <y>.
    QMap<int, QPointF> joystickAxes_;
    QStringList lines_;
    bool active_ = false;
};
