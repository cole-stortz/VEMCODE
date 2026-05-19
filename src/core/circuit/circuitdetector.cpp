#include "src/core/circuit/circuitdetector.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <map>

void CircuitDetector::detect(const std::string& source) {
    reset();

    auto defines  = parse_defines(source);
    auto pinmodes = parse_pinmodes(source, defines);

    // Phase 3 -- infer component type from name and mode
    static const std::map<ComponentType, std::string> type_labels = {
        { ComponentType::LED,           "LED"           },
        { ComponentType::Button,        "Button"        },
        { ComponentType::Switch,        "Switch"        },
        { ComponentType::Buzzer,        "Buzzer"        },
        { ComponentType::Servo,         "Servo"         },
        { ComponentType::Potentiometer, "Potentiometer" },
        { ComponentType::LightSensor,   "Light Sensor"  },
        { ComponentType::TempSensor,    "Temp Sensor"   },
        { ComponentType::AnalogSensor,  "Analog Sensor" },
        { ComponentType::LCD,           "LCD"           },
        { ComponentType::GenericOutput, "Output"        },
        { ComponentType::GenericInput,  "Input"         },
    };

    for (const auto& pm : pinmodes) {
        DetectedComponent comp;
        comp.pin       = pm.pin;
        comp.pin_name  = pm.pin_name;
        comp.type      = infer_type(pm.pin_name, pm.mode);
        comp.confirmed = false;

        auto label_it = type_labels.find(comp.type);
        std::string type_str = (label_it != type_labels.end())
            ? label_it->second : "Component";

        comp.label = comp.pin >= 0
            ? type_str + " (pin " + std::to_string(comp.pin) + ")"
            : type_str;

        bool duplicate = false;
        for (const auto& existing : components_)
            if (existing.pin == comp.pin && existing.pin >= 0)
                { duplicate = true; break; }
        if (!duplicate)
            components_.push_back(comp);
    }

    // Phase 2b -- detect components from analogRead calls
    std::regex analog_re(R"((?:api->)?analogRead\s*\(\s*(\w+)\s*\))");
    auto ab = std::sregex_iterator(source.begin(), source.end(), analog_re);
    auto ae = std::sregex_iterator();

    for (auto it = ab; it != ae; ++it) {
        std::smatch m = *it;
        std::string token = m[1].str();
        int pin = resolve_pin(token, defines);
        if (pin < 0) continue;

        bool duplicate = false;
        for (const auto& existing : components_)
            if (existing.pin == pin) { duplicate = true; break; }
        if (duplicate) continue;

        DetectedComponent comp;
        comp.pin       = pin;
        comp.pin_name  = defines.count(token) ? token : "";
        comp.type      = infer_type(token, "INPUT");
        comp.confirmed = false;

        auto label_it = type_labels.find(comp.type);
        comp.label = (label_it != type_labels.end()
                      ? label_it->second : "Analog")
                     + " (pin " + std::to_string(pin) + ")";

        components_.push_back(comp);
    }

    // Check for Serial.begin()
    if (source.find("Serial.begin") != std::string::npos ||
        source.find("Serial_begin") != std::string::npos) {
        DetectedComponent serial;
        serial.type     = ComponentType::Serial;
        serial.pin      = -1;
        serial.pin_name = "Serial";
        serial.label    = "Serial monitor";
        serial.confirmed = false;
        components_.push_back(serial);
    }
}

void CircuitDetector::confirm_pin(int pin) {
    for (auto& comp : components_) {
        if (comp.pin == pin)
            comp.confirmed = true;
    }
}

void CircuitDetector::reset() {
    components_.clear();
}

// Phase 1 -- parse #defines into a symbol table
// Handles: #define NAME VALUE and #define NAME (VALUE)
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

// Phase 2 -- find pinMode(pin, mode) calls
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

// Phase 3 -- infer component type from define name + mode
ComponentType CircuitDetector::infer_type(
    const std::string& name, const std::string& mode)
{
    std::string upper = to_upper(name);

    if (contains_any(upper, {"LED", "LIGHT", "LAMP", "INDICATOR"}))
        return ComponentType::LED;

    if (contains_any(upper, {"BTN", "BUTTON", "KEY"}))
        return ComponentType::Button;

    if (contains_any(upper, {"SWITCH", "SW", "TOGGLE"}))
        return ComponentType::Switch;

    if (contains_any(upper, {"BUZZER", "BUZZ", "SPEAKER", "TONE", "PIEZO"}))
        return ComponentType::Buzzer;

    if (contains_any(upper, {"SERVO", "MOTOR", "SRV"}))
        return ComponentType::Servo;

    if (contains_any(upper, {"POT", "POTENTIOMETER", "DIAL"}))
        return ComponentType::Potentiometer;

    if (contains_any(upper, {"PHOTO", "LDR", "LIGHT_SENSOR", "PHOTORESISTOR"}))
        return ComponentType::LightSensor;

    if (contains_any(upper, {"TEMP", "TEMPERATURE", "THERMISTOR"}))
        return ComponentType::TempSensor;

    if (contains_any(upper, {"SENSOR", "ANALOG", "ADC"}))
        return ComponentType::AnalogSensor;

    if (contains_any(upper, {"LCD", "DISPLAY", "SCREEN", "OLED"}))
        return ComponentType::LCD;

    // Fall back to mode
    if (mode == "OUTPUT")
        return ComponentType::GenericOutput;

    return ComponentType::GenericInput;
}

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