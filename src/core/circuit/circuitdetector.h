#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include "src/core/circuit/componentregistry.h"

// A single detected component
struct DetectedComponent {
    std::string type_name;
    int           pin;          // -1 for non-pin components like Serial
    std::vector<int> pins;        // for multi-pin components like LCD (RS, E, D4-D7)
    std::string   pin_name;     // original #define name e.g. "LED_PIN"
    std::string   label;        // human readable e.g. "LED (pin 13)"
    bool          confirmed;    // true once runtime pin_changed fires for this pin

    // Keypad only: pins[0..rows-1] are row pins, pins[rows..rows+cols-1] are
    // column pins. Zero for every other component type.
    int rows = 0;
    int cols = 0;

    // String return
    std::string to_string() const;
};

// Parses Arduino sketch source to infer what components are connected to which pins.
class CircuitDetector {
public:
    void detect(const std::string& source);
    void confirm_pin(int pin);
    const std::vector<DetectedComponent>& components() const { return components_; }
    const std::vector<std::string>&       warnings()   const { return warnings_; }
    void reset();

private:
    std::map<std::string, std::vector<int>> parse_arrays(const std::string& source);
    std::set<int> detect_multipin(
        const std::string& source,
        const std::map<std::string, std::string>& defines,
        const std::map<std::string, std::vector<int>>& arrays);

    void detect_generic_multipin(
        const std::map<std::string, std::string>& defines,
        const std::map<std::string, std::vector<int>>& arrays,
        std::set<int>& claimed);
    void detect_suffix_group(const ComponentDefinition& def,
        const std::map<std::string, std::string>& defines, std::set<int>& claimed);
    void detect_prefix_group(const ComponentDefinition& def,
        const std::map<std::string, std::string>& defines, std::set<int>& claimed);
    void detect_array_group(const ComponentDefinition& def,
        const std::map<std::string, std::vector<int>>& arrays, std::set<int>& claimed);
    void detect_singleton_group(const ComponentDefinition& def,
        const std::map<std::string, std::string>& defines, std::set<int>& claimed);
    void add_multipin_component(const ComponentDefinition& def,
        const std::vector<int>& pins, const std::string& group_label, std::set<int>& claimed);

    // Keypad matrix: one component whose row-pin count and col-pin count are
    // independent and read straight from the sketch, so it can't go through
    // the fixed-role-count MultiPinStrategy engine above -- handled on its own.
    void detect_keypad_matrix(const std::string& source,
        const std::map<std::string, std::string>& defines,
        const std::map<std::string, std::vector<int>>& arrays,
        std::set<int>& claimed);

    // DHT11/DHT22: "DHT dht(DHTPIN, DHTTYPE)" -- the second constructor arg is
    // a sensor-type selector, not a pin, so detect_constructor_pattern's
    // "every arg must resolve to a pin" rule can't be reused for it either.
    void detect_dht(const std::string& source,
        const std::map<std::string, std::string>& defines,
        std::set<int>& claimed);

    void detect_pattern_matches(const std::string& source,
        const std::map<std::string, std::string>& defines, std::set<int>& claimed);
    void detect_method_call_pattern(const ComponentDefinition& def, const std::string& pattern,
        const std::string& source, const std::map<std::string, std::string>& defines, std::set<int>& claimed);
    void detect_wrapper_function_pattern(const ComponentDefinition& def, const std::string& pattern,
        const std::string& source, const std::map<std::string, std::string>& defines, std::set<int>& claimed);
    void detect_constructor_pattern(const ComponentDefinition& def, const std::string& pattern,
        const std::string& source, const std::map<std::string, std::string>& defines, std::set<int>& claimed);
    std::string regex_escape(const std::string& s);

    std::vector<DetectedComponent> components_;
    std::vector<std::string>       warnings_;

    std::map<std::string, std::string> parse_defines(const std::string& source);

    struct PinModeCall {
        int         pin;
        std::string pin_name;
        std::string mode;
    };
    std::vector<PinModeCall> parse_pinmodes(
        const std::string& source,
        const std::map<std::string, std::string>& defines);

    std::string infer_type(const std::string& name, const std::string& mode);
    int resolve_pin(const std::string& token,
                    const std::map<std::string, std::string>& defines,
                    int depth = 0);
    bool contains_any(const std::string& str,
                      const std::vector<std::string>& keywords);
    bool pin_already_added(int pin) const;
    std::string to_upper(const std::string& str);
};