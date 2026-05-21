#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>

// The type of component inferred from the sketch
enum class ComponentType {
    LED,
    Button,
    Switch,
    Buzzer,
    Servo,
    Potentiometer,
    LightSensor,
    TempSensor,
    AnalogSensor,   // generic fallback
    LCD,
    GenericOutput,
    GenericInput,
    Serial,
    DistanceSensor,
    HBridgeMotor,
    ColorSensor
};

// A single detected component
struct DetectedComponent {
    ComponentType type;
    int           pin;          // -1 for non-pin components like Serial
    std::vector<int> pins;        // for multi-pin components like LCD (RS, E, D4-D7)
    std::string   pin_name;     // original #define name e.g. "LED_PIN"
    std::string   label;        // human readable e.g. "LED (pin 13)"
    bool          confirmed;    // true once runtime pin_changed fires for this pin
};

// CircuitDetector parses Arduino sketch source and infers
// what components are connected to which pins.
//
// Usage:
//   CircuitDetector detector;
//   detector.detect(source_code);
//   auto components = detector.components();
class CircuitDetector {
public:
    // Run detection on sketch source text.
    // Can be called again when the source changes.
    void detect(const std::string& source);

    // Confirm a component at runtime when a pin actually fires.
    // Called by the host when on_pin_changed fires.
    void confirm_pin(int pin);

    // Returns all detected components after detect() is called
    const std::vector<DetectedComponent>& components() const { return components_; }

    // Clear all state
    void reset();

private:
    // Parses: const int NAME[N] = {v1, v2, v3};
    std::map<std::string, std::vector<int>> parse_arrays(const std::string& source);

    // Phase 0: detect multi-pin components, returns set of claimed pins
    std::set<int> detect_multipin(
        const std::string& source,
        const std::map<std::string, std::string>& defines,
        const std::map<std::string, std::vector<int>>& arrays);

    std::vector<DetectedComponent> components_;

    // Phase 1: parse all #define NAME VALUE into a symbol table
    std::map<std::string, std::string> parse_defines(const std::string& source);

    // Phase 2: find all pinMode(x, mode) calls, resolve x through defines
    struct PinModeCall {
        int         pin;
        std::string pin_name;   // original name if from a #define, else ""
        std::string mode;       // "OUTPUT", "INPUT", "INPUT_PULLUP"
    };
    std::vector<PinModeCall> parse_pinmodes(
        const std::string& source,
        const std::map<std::string, std::string>& defines);

    // Phase 3: infer component type from name + mode
    ComponentType infer_type(const std::string& name, const std::string& mode);

    // Resolve a pin value -- either a number or a #define name
    int resolve_pin(const std::string& token,
                    const std::map<std::string, std::string>& defines);

    // Check if a string contains any of the given keywords (case-insensitive)
    bool contains_any(const std::string& str,
                      const std::vector<std::string>& keywords);

    std::string to_upper(const std::string& str);
};