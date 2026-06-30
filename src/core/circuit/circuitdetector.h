#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>

// A single detected component
struct DetectedComponent {
    std::string type_name;
    int           pin;          // -1 for non-pin components like Serial
    std::vector<int> pins;        // for multi-pin components like LCD (RS, E, D4-D7)
    std::string   pin_name;     // original #define name e.g. "LED_PIN"
    std::string   label;        // human readable e.g. "LED (pin 13)"
    bool          confirmed;    // true once runtime pin_changed fires for this pin

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
                    const std::map<std::string, std::string>& defines);
    bool contains_any(const std::string& str,
                      const std::vector<std::string>& keywords);
    bool pin_already_added(int pin) const;
    std::string to_upper(const std::string& str);
};