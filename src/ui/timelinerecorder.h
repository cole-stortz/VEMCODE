#pragma once
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <QPointF>
#include <vector>
#include "src/core/circuit/circuitdetector.h"
#include "src/core/circuit/componentitem.h"

// Captures canvas interactions and Serial sends into `.timeline`-format
// lines (see src/core/host/timeline.h for the format this mirrors, and
// TestRunner::dispatchAction there for the verb/arg shape per component
// type) while active, so a manual GUI run can be exported and replayed
// headlessly. Purely a writer -- src/core/host/timeline.* is the reader.
class TimelineRecorder {
public:
    void clear();
    void setActive(bool active) { active_ = active; }
    bool isActive() const { return active_; }
    bool isEmpty() const { return lines_.isEmpty(); }

    // Not every ComponentEventType/type_name combination has a matching
    // verb in TestRunner::dispatchAction (Keypad, DHT) -- those are
    // silently skipped rather than producing an unparseable line.
    void recordComponentInput(int pin, ComponentEventType type, const QVariant& value,
                               double time_s, const std::vector<DetectedComponent>& components);
    void recordSerialSend(const QString& text, double time_s);

    bool exportToFile(const QString& path) const;

private:
    const DetectedComponent* findComponentForPin(int pin,
        const std::vector<DetectedComponent>& components) const;
    void appendLine(double time_s, const QString& verb, const QString& target,
                     const QStringList& args = {});

    // Per-joystick last-known axis value (keyed by the joystick's VRX pin,
    // i.e. pins[0]), since a single-axis AnalogValue event still needs a
    // full MOVE <x> <y> -- the format has no per-axis SET for Joystick.
    QMap<int, QPointF> joystickAxes_;
    QStringList lines_;
    bool active_ = false;
};
