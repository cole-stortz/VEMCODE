#include "src/core/circuit/circuitdetector.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>

// -------------------------------------------------------
// detect() -- main entry point, runs all three phases
// -------------------------------------------------------
void CircuitDetector::detect(const std::string& source) {
    reset();

    // Phase 1 -- build symbol table from #defines
    auto defines = parse_defines(source);

    // Phase 2 -- find pinMode calls and resolve pin numbers
    auto pinmodes = parse_pinmodes(source, defines);

    // Phase 3 -- infer component type from name and mode
    for (const auto& pm : pinmodes) {
        DetectedComponent comp;
        comp.pin       = pm.pin;
        comp.pin_name  = pm.pin_name;
        comp.type      = infer_type(pm.pin_name, pm.mode);
        comp.confirmed = false;

        // Build a human readable label
        std::string type_str;
        switch (comp.type) {
            case ComponentType::LED:          type_str = "LED";          break;
            case ComponentType::Button:       type_str = "Button";       break;
            case ComponentType::Buzzer:       type_str = "Buzzer";       break;
            case ComponentType::Servo:        type_str = "Servo";        break;
            case ComponentType::Potentiometer:type_str = "Potentiometer";break;
            case ComponentType::LCD:          type_str = "LCD";          break;
            case ComponentType::GenericOutput:type_str = "Output";       break;
            case ComponentType::GenericInput: type_str = "Input";        break;
            default:                          type_str = "Component";    break;
        }

        if (comp.pin >= 0)
            comp.label = type_str + " (pin " + std::to_string(comp.pin) + ")";
        else
            comp.label = type_str;

        // Avoid duplicates -- same pin already detected
        bool duplicate = false;
        for (const auto& existing : components_) {
            if (existing.pin == comp.pin && existing.pin >= 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            components_.push_back(comp);
    }

    // Check for Serial.begin() -- not pin-based
    if (source.find("Serial.begin") != std::string::npos ||
        source.find("Serial_begin") != std::string::npos) {
        DetectedComponent serial;
        serial.type      = ComponentType::Serial;
        serial.pin       = -1;
        serial.pin_name  = "Serial";
        serial.label     = "Serial monitor";
        serial.confirmed = false;
        components_.push_back(serial);
    }
}

// -------------------------------------------------------
// confirm_pin() -- marks a component confirmed at runtime
// -------------------------------------------------------
void CircuitDetector::confirm_pin(int pin) {
    for (auto& comp : components_) {
        if (comp.pin == pin)
            comp.confirmed = true;
    }
}

void CircuitDetector::reset() {
    components_.clear();
}

// -------------------------------------------------------
// Phase 1 -- parse #defines into a symbol table
// Handles: #define NAME VALUE and #define NAME (VALUE)
// -------------------------------------------------------
std::map<std::string, std::string> CircuitDetector::parse_defines(
    const std::string& source)
{
    std::map<std::string, std::string> defines;

    // Match: #define IDENTIFIER value
    std::regex pattern(R"(#\s*define\s+(\w+)\s+\(?\s*(\w+)\s*\)?)");
    auto begin = std::sregex_iterator(source.begin(), source.end(), pattern);
    auto end   = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch m = *it;
        defines[m[1].str()] = m[2].str();
    }

    return defines;
}

// -------------------------------------------------------
// Phase 2 -- find pinMode(pin, mode) calls
// -------------------------------------------------------
std::vector<CircuitDetector::PinModeCall> CircuitDetector::parse_pinmodes(
    const std::string& source,
    const std::map<std::string, std::string>& defines)
{
    std::vector<PinModeCall> calls;

    // Match: pinMode(token, MODE) or api->pinMode(token, MODE)
    std::regex pattern(
        R"((?:api->)?pinMode\s*\(\s*(\w+)\s*,\s*(INPUT_PULLUP|INPUT|OUTPUT)\s*\))"
    );
    auto begin = std::sregex_iterator(source.begin(), source.end(), pattern);
    auto end   = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch m = *it;
        std::string token = m[1].str();
        std::string mode  = m[2].str();

        PinModeCall call;
        call.mode = mode;
        call.pin  = resolve_pin(token, defines);

        // If token was a #define name, keep it for type inference
        if (defines.count(token))
            call.pin_name = token;
        else
            call.pin_name = "";

        if (call.pin >= 0)
            calls.push_back(call);
    }

    return calls;
}

// -------------------------------------------------------
// Phase 3 -- infer component type from define name + mode
// -------------------------------------------------------
ComponentType CircuitDetector::infer_type(
    const std::string& name, const std::string& mode)
{
    std::string upper = to_upper(name);

    if (contains_any(upper, {"LED", "LIGHT", "LAMP", "INDICATOR"}))
        return ComponentType::LED;

    if (contains_any(upper, {"BTN", "BUTTON", "SWITCH", "SW", "KEY"}))
        return ComponentType::Button;

    if (contains_any(upper, {"BUZZER", "BUZZ", "SPEAKER", "TONE", "PIEZO"}))
        return ComponentType::Buzzer;

    if (contains_any(upper, {"SERVO", "MOTOR", "SRV"}))
        return ComponentType::Servo;

    if (contains_any(upper, {"POT", "SENSOR", "ANALOG", "ADC", "PHOTO",
                              "TEMP", "LIGHT_SENSOR", "LDR"}))
        return ComponentType::Potentiometer;

    if (contains_any(upper, {"LCD", "DISPLAY", "SCREEN", "OLED"}))
        return ComponentType::LCD;

    // Fall back to mode
    if (mode == "OUTPUT")
        return ComponentType::GenericOutput;

    return ComponentType::GenericInput;
}

// -------------------------------------------------------
// Helpers
// -------------------------------------------------------
int CircuitDetector::resolve_pin(
    const std::string& token,
    const std::map<std::string, std::string>& defines)
{
    // Direct number
    if (!token.empty() && std::isdigit(token[0]))
        return std::stoi(token);

    // Arduino analog pin A0-A7 = 14-21
    if (token.size() >= 2 && token[0] == 'A' && std::isdigit(token[1]))
        return 14 + (token[1] - '0');

    // Look up in defines
    auto it = defines.find(token);
    if (it != defines.end())
        return resolve_pin(it->second, defines); // recurse in case of chained defines

    return -1; // unresolvable
}

bool CircuitDetector::contains_any(
    const std::string& str,
    const std::vector<std::string>& keywords)
{
    for (const auto& kw : keywords) {
        if (str.find(kw) != std::string::npos)
            return true;
    }
    return false;
}

std::string CircuitDetector::to_upper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}