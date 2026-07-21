#pragma once
#include "src/core/circuit/circuitdetector.h"
#include <string>
#include <vector>
#include <map>

class SketchHost;

// One line of a `.timeline` sidecar file: `time, VERB, target, [args...]`.
struct TimelineEvent {
    double time;                    // sketch-time seconds
    std::string verb;               // ASSERT, or an action verb (PRESS, SET, SEND, ...)
    std::string target;             // component pin_name, a reserved keyword (PIN/SERIAL/WIRE/SPI/
                                     // SERIAL_CONTAINS), or a literal pin number
    std::vector<std::string> args;
    int line_number;
};

// Parses a `.timeline` file. Blank lines and lines starting with '#' are
// skipped. Throws std::runtime_error (message prefixed "<path>:<line>: ")
// on any malformed line.
std::vector<TimelineEvent> parse_timeline_file(const std::string& path);

// Drives a running sketch through a parsed timeline: fires action events
// (button presses, analog values, Serial data, ...) via SketchHost's
// inject_* methods and checks ASSERT events against pin state / Serial
// output captured through ArduinoRuntime's on_pin_changed/on_serial_output
// callbacks.
class TestRunner {
public:
    TestRunner(SketchHost& host, std::vector<DetectedComponent> components,
               std::vector<TimelineEvent> events);

    // Fed by the caller's own on_pin_changed/on_serial_output callbacks --
    // not installed directly, since ArduinoRuntime only has one callback
    // slot of each and run_headless's own callbacks (stdout echo) need to
    // keep running too.
    void onPinChanged(int pin, int value) { pinState_[pin] = value; }
    void onSerialOutput(const std::string& text) { serialBuffer_ += text; }

    // Fires at most one due event (time <= sketchSeconds) per call -- never
    // more, even if several are already due -- so the sketch's own loop()
    // always gets to run (via the caller's host.run_loop(), once per
    // main-loop iteration) between any two consecutive events. Call once per
    // main-loop iteration, inside the same lock that guards host.run_loop().
    void fireDueEvents(double sketchSeconds);

    bool finished() const { return nextEvent_ >= events_.size(); }
    bool anyAssertFailed() const { return assertFailed_ > 0; }
    void printSummary() const;

private:
    void dispatchAction(const TimelineEvent& ev);
    void checkAssert(const TimelineEvent& ev);
    const DetectedComponent* resolve(const std::string& target) const;

    SketchHost& host_;
    std::vector<DetectedComponent> components_;
    std::vector<TimelineEvent> events_;
    size_t nextEvent_ = 0;

    std::map<int, int> pinState_;
    std::string serialBuffer_;
    int assertCount_  = 0;
    int assertFailed_ = 0;
};
