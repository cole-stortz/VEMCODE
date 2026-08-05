#pragma once
#include "src/core/circuit/circuitdetector.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

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

// Parses a `.timeline` file; blank lines and lines starting with '#' are skipped.
// Throws std::runtime_error (message prefixed "<path>:<line>: ") on malformed lines.
std::vector<TimelineEvent> parse_timeline_file(const std::string& path);

// Drives a running sketch through a parsed timeline: fires action events via SketchHost's
// inject_* methods, checks ASSERTs against pin state / Serial output from ArduinoRuntime callbacks.
class TestRunner {
public:
    TestRunner(SketchHost& host, std::vector<DetectedComponent> components,
               std::vector<TimelineEvent> events);

    // Fed by the caller's own on_pin_changed/on_serial_output callbacks, not installed
    // directly -- ArduinoRuntime has only one slot of each, and run_headless's stdout echo needs it too.
    void onPinChanged(int pin, int value) { pinState_[pin] = value; }
    void onSerialOutput(const std::string& text) { serialBuffer_ += text; }

    // Fires at most one due event per call (never a whole batch) so the sketch's own loop()
    // gets to run between events. Call once per main-loop iteration, inside the run_loop() lock.
    void fireDueEvents(double sketchSeconds);

    bool finished() const { return nextEvent_ >= events_.size(); }
    bool anyAssertFailed() const { return assertFailed_ > 0; }
    int assertCount() const { return assertCount_; }
    int assertFailedCount() const { return assertFailed_; }
    void printSummary() const;

    // Optional: called from checkAssert() alongside the stdout PASS/FAIL line so a GUI
    // caller doesn't need to scrape stdout. Not used by run_headless.
    std::function<void(bool pass, double time, const std::string& message)> on_assert_result;

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
