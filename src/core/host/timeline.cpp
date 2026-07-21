#include "src/core/host/timeline.h"
#include "src/core/host/sketchhost.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::toupper);
    return out;
}

// Strips a '#' comment and everything after it, whether it starts the line
// or trails real fields -- but not one that appears inside a quoted string
// (so `SEND, SERIAL, "text #1"` keeps its '#').
std::string stripComment(const std::string& line) {
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '\\' && inQuotes && i + 1 < line.size()) { ++i; continue; }
        if (c == '"') inQuotes = !inQuotes;
        else if (c == '#' && !inQuotes) return line.substr(0, i);
    }
    return line;
}

// Splits a line on commas, respecting double-quoted fields (so a comma or
// leading/trailing space inside "..." isn't treated as a field separator).
// Quotes are stripped from the returned field; \" and \\ are unescaped.
std::vector<std::string> splitFields(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '\\' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\')) {
                current += line[++i];
            } else if (c == '"') {
                inQuotes = false;
            } else {
                current += c;
            }
        } else if (c == '"') {
            inQuotes = true;
        } else if (c == ',') {
            fields.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    fields.push_back(trim(current));
    return fields;
}

bool tryParseInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    try {
        size_t pos = 0;
        out = std::stoi(s, &pos, 0); // base 0 auto-detects "0x.." / decimal
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

int parsePinValue(const std::string& s) {
    std::string up = toUpper(s);
    if (up == "HIGH") return 1;
    if (up == "LOW")  return 0;
    int v = 0;
    if (!tryParseInt(s, v))
        throw std::runtime_error("expected HIGH, LOW, or an integer, got '" + s + "'");
    return v;
}

} // namespace

std::vector<TimelineEvent> parse_timeline_file(const std::string& path) {
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error(path + ": can't open timeline file");

    std::vector<TimelineEvent> events;
    std::string line;
    int lineNo = 0;

    while (std::getline(file, line)) {
        ++lineNo;
        std::string trimmed = trim(stripComment(line));
        if (trimmed.empty()) continue;

        std::vector<std::string> fields = splitFields(trimmed);
        if (fields.size() < 3) {
            throw std::runtime_error(path + ":" + std::to_string(lineNo) +
                ": expected at least 'time, VERB, target', got " +
                std::to_string(fields.size()) + " field(s)");
        }

        TimelineEvent ev;
        ev.line_number = lineNo;
        try {
            size_t pos = 0;
            ev.time = std::stod(fields[0], &pos);
            if (pos != fields[0].size()) throw std::invalid_argument("");
        } catch (...) {
            throw std::runtime_error(path + ":" + std::to_string(lineNo) +
                ": expected a numeric time, got '" + fields[0] + "'");
        }
        ev.verb   = fields[1];
        ev.target = fields[2];
        ev.args.assign(fields.begin() + 3, fields.end());

        events.push_back(std::move(ev));
    }

    std::stable_sort(events.begin(), events.end(),
        [](const TimelineEvent& a, const TimelineEvent& b) { return a.time < b.time; });

    return events;
}

TestRunner::TestRunner(SketchHost& host, std::vector<DetectedComponent> components,
                        std::vector<TimelineEvent> events)
    : host_(host), components_(std::move(components)), events_(std::move(events))
{}

const DetectedComponent* TestRunner::resolve(const std::string& target) const {
    for (const auto& c : components_)
        if (c.pin_name == target) return &c;
    return nullptr;
}

void TestRunner::fireDueEvents(double sketchSeconds) {
    // Fires at most ONE due event per call, not every event that's become due
    // -- the caller runs host.run_loop() once after each call, so this
    // guarantees the sketch gets to react (e.g. re-read a just-injected pin
    // and re-drive an output) between any two consecutive timeline events,
    // even if both are already due by the same sketchSeconds (which speed=N
    // makes easy to hit: two events 0.05s apart in sketch-time are only
    // 0.0125s apart in real time at speed=4). Firing them all in one batch
    // would let an ASSERT race ahead and check stale pre-action state.
    if (nextEvent_ < events_.size() && events_[nextEvent_].time <= sketchSeconds) {
        const TimelineEvent& ev = events_[nextEvent_];
        try {
            if (toUpper(ev.verb) == "ASSERT")
                checkAssert(ev);
            else
                dispatchAction(ev);
        } catch (const std::exception& e) {
            std::cerr << "WARNING: timeline line " << ev.line_number << ": " << e.what() << "\n";
        }
        ++nextEvent_;
    }
}

void TestRunner::checkAssert(const TimelineEvent& ev) {
    std::string target = toUpper(ev.target);

    if (target == "PIN") {
        if (ev.args.size() < 2)
            throw std::runtime_error("ASSERT, PIN needs a pin and an expected value");

        int pin = 0;
        if (!tryParseInt(ev.args[0], pin)) {
            const DetectedComponent* comp = resolve(ev.args[0]);
            if (!comp)
                throw std::runtime_error("unknown pin/component '" + ev.args[0] + "'");
            pin = comp->pin;
        }

        int expected = parsePinValue(ev.args[1]);
        auto it = pinState_.find(pin);
        int actual = (it != pinState_.end()) ? it->second : 0;

        ++assertCount_;
        bool pass = (actual == expected);
        if (!pass) ++assertFailed_;
        std::cout << (pass ? "PASS" : "FAIL") << " [t=" << ev.time << "] ASSERT PIN "
                   << ev.args[0] << " == " << ev.args[1]
                   << " (actual: " << actual << ")\n";
        return;
    }

    if (target == "SERIAL_CONTAINS") {
        if (ev.args.empty())
            throw std::runtime_error("ASSERT, SERIAL_CONTAINS needs a text argument");

        ++assertCount_;
        bool pass = serialBuffer_.find(ev.args[0]) != std::string::npos;
        if (!pass) ++assertFailed_;
        std::cout << (pass ? "PASS" : "FAIL") << " [t=" << ev.time
                   << "] ASSERT SERIAL_CONTAINS \"" << ev.args[0] << "\"\n";
        return;
    }

    throw std::runtime_error("unknown ASSERT target '" + ev.target + "' "
        "(expected PIN or SERIAL_CONTAINS)");
}

void TestRunner::dispatchAction(const TimelineEvent& ev) {
    std::string target = toUpper(ev.target);
    std::string verb    = toUpper(ev.verb);

    if (target == "SERIAL") {
        if (verb != "SEND" || ev.args.empty())
            throw std::runtime_error("SERIAL only supports SEND \"text\"");
        host_.inject_serial(ev.args[0]);
        return;
    }

    if (target == "WIRE") {
        if (verb != "SET_RESPONSE" || ev.args.size() < 1)
            throw std::runtime_error("WIRE only supports SET_RESPONSE <address> [byte...]");
        int address = 0;
        if (!tryParseInt(ev.args[0], address))
            throw std::runtime_error("bad WIRE address '" + ev.args[0] + "'");
        std::vector<uint8_t> bytes;
        for (size_t i = 1; i < ev.args.size(); ++i) {
            int v = 0;
            if (tryParseInt(ev.args[i], v)) bytes.push_back((uint8_t)(v & 0xFF));
        }
        host_.inject_wire_device(address, bytes);
        return;
    }

    if (target == "SPI") {
        if (verb != "SET_RESPONSE")
            throw std::runtime_error("SPI only supports SET_RESPONSE [byte...]");
        std::vector<uint8_t> bytes;
        for (const auto& a : ev.args) {
            int v = 0;
            if (tryParseInt(a, v)) bytes.push_back((uint8_t)(v & 0xFF));
        }
        host_.inject_spi_bytes(bytes);
        return;
    }

    int literalPin = 0;
    if (tryParseInt(ev.target, literalPin)) {
        if (verb != "SEND" || ev.args.empty())
            throw std::runtime_error("a literal pin target only supports SEND \"text\" (SoftwareSerial RX)");
        host_.runtime().inject_soft_serial(literalPin, ev.args[0]);
        return;
    }

    const DetectedComponent* comp = resolve(ev.target);
    if (!comp)
        throw std::runtime_error("unknown target '" + ev.target + "' -- not a detected "
            "component, and not PIN/SERIAL/WIRE/SPI/a literal pin number");

    const std::string& type = comp->type_name;

    if (type == "Button" || type == "ButtonClean") {
        if (verb == "PRESS")        host_.inject_button_bounce(comp->pin, 1);
        else if (verb == "RELEASE") host_.inject_button_bounce(comp->pin, 0);
        else throw std::runtime_error("Button only supports PRESS/RELEASE");
        return;
    }

    if (type == "Switch") {
        if (verb == "PRESS")        host_.inject_pin(comp->pin, 1);
        else if (verb == "RELEASE") host_.inject_pin(comp->pin, 0);
        else if (verb == "SET" && !ev.args.empty())
            host_.inject_pin(comp->pin, parsePinValue(ev.args[0]));
        else throw std::runtime_error("Switch only supports PRESS/RELEASE/SET <0|1>");
        return;
    }

    if (type == "Potentiometer" || type == "GenericInput" ||
        type == "LightSensor" || type == "TemperatureSensor" || type == "ForceSensor" ||
        type == "AnalogSensor") {
        if (verb != "SET" || ev.args.empty())
            throw std::runtime_error(type + " only supports SET <value>");
        int v = 0;
        if (!tryParseInt(ev.args[0], v)) throw std::runtime_error("bad SET value '" + ev.args[0] + "'");
        host_.inject_analog(comp->pin, v);
        return;
    }

    if (type == "IR Sensor") {
        // Digital (DigitalPress), not pulseIn-based -- see ir_sensor.cpp.
        if (verb == "PRESS")        host_.inject_pin(comp->pin, 1);
        else if (verb == "RELEASE") host_.inject_pin(comp->pin, 0);
        else if (verb == "SET" && !ev.args.empty())
            host_.inject_pin(comp->pin, parsePinValue(ev.args[0]));
        else throw std::runtime_error("IR Sensor only supports PRESS/RELEASE/SET <0|1>");
        return;
    }

    if (type == "DistanceSensor") {
        if (verb != "SET" || ev.args.empty())
            throw std::runtime_error("DistanceSensor only supports SET <cm>");
        double cm = 0;
        try { cm = std::stod(ev.args[0]); }
        catch (...) { throw std::runtime_error("bad SET value '" + ev.args[0] + "'"); }
        if (cm < 0) throw std::runtime_error("distance can't be negative");
        // Same cm -> pulseIn() microseconds conversion the GUI's distance-sensor
        // input widget uses (distance_sensor.cpp), so timelines can say "10"
        // (cm) instead of a pre-computed microsecond value.
        unsigned long micros = (unsigned long)std::ceil(cm * 2.0 / 0.034);
        host_.inject_pulse_duration(comp->pin, micros);
        return;
    }

    if (type == "Joystick") {
        if (verb == "MOVE") {
            if (ev.args.size() < 2 || comp->pins.size() < 2)
                throw std::runtime_error("Joystick MOVE needs <x> <y>");
            int x = 0, y = 0;
            if (!tryParseInt(ev.args[0], x) || !tryParseInt(ev.args[1], y))
                throw std::runtime_error("bad MOVE x/y value");
            host_.inject_analog(comp->pins[0] /* VRX */, x);
            host_.inject_analog(comp->pins[1] /* VRY */, y);
        } else if ((verb == "PRESS" || verb == "RELEASE") && comp->pins.size() >= 3) {
            host_.inject_button_bounce(comp->pins[2] /* SW */, verb == "PRESS" ? 1 : 0);
        } else {
            throw std::runtime_error("Joystick only supports MOVE <x> <y> / PRESS / RELEASE");
        }
        return;
    }

    if (type == "ColorSensor") {
        if (verb != "SET_COLOR" || ev.args.size() < 3 || comp->pins.size() < 3)
            throw std::runtime_error("ColorSensor only supports SET_COLOR <r> <g> <b>");
        int r = 0, g = 0, b = 0;
        if (!tryParseInt(ev.args[0], r) || !tryParseInt(ev.args[1], g) || !tryParseInt(ev.args[2], b))
            throw std::runtime_error("bad SET_COLOR r/g/b value");
        host_.inject_color(comp->pins[0] /* OUT */, comp->pins[1] /* S2 */, comp->pins[2] /* S3 */, r, g, b);
        return;
    }

    throw std::runtime_error("'" + type + "' has no injectable action verbs (output-only component)");
}

void TestRunner::printSummary() const {
    std::cout << "\n=== Timeline summary ===\n";
    std::cout << assertCount_ - assertFailed_ << "/" << assertCount_ << " assertions passed";
    if (assertFailed_ > 0)
        std::cout << " -- " << assertFailed_ << " FAILED";
    std::cout << "\n";
    if (nextEvent_ < events_.size())
        std::cout << "WARNING: " << (events_.size() - nextEvent_)
                   << " timeline event(s) never fired (ran out of time)\n";
}
