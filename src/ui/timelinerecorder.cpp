#include "src/ui/timelinerecorder.h"
#include <QFile>
#include <QTextStream>

void TimelineRecorder::clear() {
    lines_.clear();
    joystickAxes_.clear();
}

const DetectedComponent* TimelineRecorder::findComponentForPin(
    int pin, const std::vector<DetectedComponent>& components) const {
    for (const DetectedComponent& c : components) {
        if (c.pin == pin) return &c;
        for (int p : c.pins) if (p == pin) return &c;
    }
    return nullptr;
}

static QString quoteEscape(const QString& text) {
    QString escaped = text;
    escaped.replace("\\", "\\\\").replace("\"", "\\\"");
    return "\"" + escaped + "\"";
}

void TimelineRecorder::appendLine(double time_s, const QString& verb, const QString& target,
                                   const QStringList& args) {
    QString line = QString("%1, %2, %3").arg(time_s, 0, 'f', 3).arg(verb, target);
    for (const QString& arg : args) line += ", " + arg;
    lines_.append(line);
}

void TimelineRecorder::recordComponentInput(int pin, ComponentEventType type, const QVariant& value,
                                             double time_s, const std::vector<DetectedComponent>& components) {
    const DetectedComponent* comp = findComponentForPin(pin, components);
    if (!comp) return;
    QString target = QString::fromStdString(comp->pin_name);
    const std::string& t = comp->type_name;

    switch (type) {
        case ComponentEventType::BouncePress:
            // Button: idle-HIGH INPUT_PULLUP wiring, matches button.cpp
            // (BouncePress, 0 on press / BouncePress, 1 on release).
            if (t == "Button")
                appendLine(time_s, value.toInt() == 0 ? "PRESS" : "RELEASE", target);
            break;

        case ComponentEventType::DigitalPress:
            // ButtonClean shares Button's polarity (0=press/1=release), but
            // via DigitalPress since it's exempt from bounce simulation.
            if (t == "ButtonClean")
                appendLine(time_s, value.toInt() == 0 ? "PRESS" : "RELEASE", target);
            // Switch, IR Sensor, and a Joystick's SW pin (also DigitalPress,
            // see joystick.cpp) all share 1=press/0=release polarity.
            else if (t == "Switch" || t == "IR Sensor" || t == "Joystick")
                appendLine(time_s, value.toInt() == 1 ? "PRESS" : "RELEASE", target);
            break;

        case ComponentEventType::AnalogValue:
            if (t == "Joystick" && comp->pins.size() >= 2) {
                // No per-axis SET for Joystick -- only MOVE <x> <y> -- so
                // track the other axis's last value and always emit both.
                int xPin = comp->pins[0];
                QPointF& axes = joystickAxes_[xPin];
                if (pin == comp->pins[0])      axes.setX(value.toInt());
                else if (pin == comp->pins[1]) axes.setY(value.toInt());
                appendLine(time_s, "MOVE", target,
                    {QString::number((int)axes.x()), QString::number((int)axes.y())});
            } else {
                appendLine(time_s, "SET", target, {QString::number(value.toInt())});
            }
            break;

        case ComponentEventType::PulseUs:
            if (t == "DistanceSensor") {
                // Inverse of the cm -> pulseIn(us) conversion both the GUI
                // widget (distance_sensor.cpp) and dispatchAction use.
                double cm = value.toULongLong() * 0.034 / 2.0;
                appendLine(time_s, "SET", target, {QString::number(cm, 'f', 1)});
            }
            break;

        case ComponentEventType::ColorRGB: {
            QVariantList c = value.toList();
            appendLine(time_s, "SET_COLOR", target,
                {QString::number(c.value(0).toInt()),
                 QString::number(c.value(1).toInt()),
                 QString::number(c.value(2).toInt())});
            break;
        }

        case ComponentEventType::KeypadWiring:
        case ComponentEventType::KeypadPress:
        case ComponentEventType::DhtReading:
            // No matching verb in TestRunner::dispatchAction -- not
            // recordable yet.
            break;
    }
}

void TimelineRecorder::recordSerialSend(const QString& text, double time_s) {
    appendLine(time_s, "SEND", "SERIAL", {quoteEscape(text)});
}

bool TimelineRecorder::exportToFile(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    for (const QString& line : lines_) out << line << "\n";
    return true;
}
