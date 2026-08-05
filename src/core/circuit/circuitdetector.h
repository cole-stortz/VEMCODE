#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include "src/core/circuit/componentregistry.h"
#include "src/core/circuit/i2cbus.h"

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

    // Max7219 only: number of daisy-chained 8x8 devices (LedControl's 4th
    // constructor arg). 1 for every other component type.
    int num_devices = 1;

    // NeoPixel only: LED count (Adafruit_NeoPixel's 1st constructor arg).
    // 1 for every other component type.
    int strip_length = 1;

    // OLED only: pixel dimensions (Adafruit_SSD1306's 1st/2nd constructor
    // args). 128x64 for every other component type (harmless default).
    int display_width  = 128;
    int display_height = 64;

    std::string to_string() const;
};

// Parses Arduino sketch source to infer what components are connected to which pins.
class CircuitDetector {
public:
    // max_pin: highest valid pin for the board; rejects non-pin #defines sharing a keyword
    // prefix with a real pin (see detect_prefix_group). Defaults to Mega's range (largest board).
    void detect(const std::string& source, int max_pin = 69);
    void confirm_pin(int pin);
    const std::vector<DetectedComponent>& components() const { return components_; }
    const std::vector<std::string>&       warnings()   const { return warnings_; }
    void reset();

    // Available to ComponentDefinition::detect_custom callbacks.
    int resolve_pin(const std::string& token,
                    const std::map<std::string, std::string>& defines,
                    int depth = 0);
    bool contains_any(const std::string& str,
                      const std::vector<std::string>& keywords);
    bool pin_already_added(int pin) const;
    std::string to_upper(const std::string& str);
    void add_detected_component(const DetectedComponent& comp, std::set<int>& claimed);

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

    // OLED (SSD1306, I2C): resetPin is often -1 (no reset line on I2C breakouts), leaving no
    // GPIO to key the canvas item by, so it falls back to a fixed sentinel
    // (Adafruit_SSD1306::NO_RESET_PIN_KEY in ssd1306.inc, kept in sync by hand).
    void detect_oled(const std::string& source,
        const std::map<std::string, std::string>& defines,
        std::set<int>& claimed);

    void detect_custom_components(const std::string& source,
        const std::map<std::string, std::string>& defines,
        const std::map<std::string, std::vector<int>>& arrays,
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
    int max_pin_ = 69;

    // Synthetic pin for I2C devices with no real GPIO (see detect_oled); increments per
    // assignment so multiple such devices get distinct keys instead of colliding.
    int next_i2c_bus_pin_ = I2C_BUS_PIN_BASE;

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
};