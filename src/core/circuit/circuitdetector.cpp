#include "src/core/circuit/circuitdetector.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <map>

void CircuitDetector::detect(const std::string& source) {
    reset();

    auto defines  = parse_defines(source);
    auto arrays   = parse_arrays(source);                         
    auto claimed  = detect_multipin(source, defines, arrays);     
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
        if (claimed.count(pm.pin)) continue;
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
        if (claimed.count(pin)) continue;

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

    // Post-process: append [n] index to labels when multiple components share the same type
    std::map<ComponentType, int> type_counts;
    for (const auto& c : components_)
        type_counts[c.type]++;

    std::map<ComponentType, int> type_idx;
    for (auto& c : components_) {
        if (type_counts[c.type] > 1) {
            int idx = type_idx[c.type]++;
            // Insert [n] before the " (pin ...)" suffix, or append if no suffix
            auto paren = c.label.find(" (");
            if (paren != std::string::npos)
                c.label.insert(paren, "[" + std::to_string(idx) + "]");
            else
                c.label += "[" + std::to_string(idx) + "]";
        }
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

// Phase 1 -- parse #defines and const int scalars into a symbol table
// Handles: #define NAME VALUE, #define NAME (VALUE), and const int NAME = VALUE;
std::map<std::string, std::string> CircuitDetector::parse_defines(
    const std::string& source)
{
    std::map<std::string, std::string> defines;

    // Match: #define IDENTIFIER value
    std::regex define_re(R"(#\s*define\s+(\w+)\s+\(?\s*(\w+)\s*\)?)");
    auto begin = std::sregex_iterator(source.begin(), source.end(), define_re);
    auto end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::smatch m = *it;
        defines[m[1].str()] = m[2].str();
    }

    // Also match: const int NAME = VALUE; (single value, not arrays)
    // This lets pin names like "const int trigPin1 = 9;" resolve just like #defines.
    std::regex const_int_re(R"(const\s+int\s+(\w+)\s*=\s*(\d+)\s*;)");
    begin = std::sregex_iterator(source.begin(), source.end(), const_int_re);
    end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::smatch m = *it;
        defines[m[1].str()] = m[2].str();
    }

    return defines;
}

std::map<std::string, std::vector<int>> CircuitDetector::parse_arrays(
    const std::string& source)
{
    std::map<std::string, std::vector<int>> arrays;
    std::map<std::string, std::string> empty_defines;

    // Match: const int NAME[anything] = {v1, v2, v3, v4};
    std::regex pattern(R"(const\s+int\s+(\w+)\s*\[.*?\]\s*=\s*\{([^}]+)\})");
    auto begin = std::sregex_iterator(source.begin(), source.end(), pattern);
    auto end   = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch m = *it;
        std::string name     = m[1].str();
        std::string contents = m[2].str();  // "33, 31, 14, 19"

        // Split contents on commas, resolve each token to a pin number
        std::vector<int> pins;
        std::stringstream ss(contents);
        std::string token;
        while (std::getline(ss, token, ',')) {
            // trim whitespace
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            int pin = resolve_pin(token, empty_defines);
            pins.push_back(pin);
        }
        arrays[name] = pins;
    }
    return arrays;
}

std::set<int> CircuitDetector::detect_multipin(
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    const std::map<std::string, std::vector<int>>& arrays)
{
    std::set<int> claimed;

    // --- HC-SR04 (multiple sensors supported) ---
    // Collect all TRIG and ECHO defines, keyed by the suffix after the keyword
    // e.g. "TRIGPIN1" → suffix "PIN1", "ECHOPIN1" → suffix "PIN1" → they pair up
    std::map<std::string, int> trig_map;  // suffix → pin
    std::map<std::string, int> echo_map;
    for (const auto& d : defines) {
        std::string upper = to_upper(d.first);
        int pin = resolve_pin(d.second, defines);
        if (pin < 0) continue;
        size_t pos;
        if ((pos = upper.find("TRIG")) != std::string::npos)
            trig_map[upper.substr(pos + 4)] = pin;
        else if ((pos = upper.find("ECHO")) != std::string::npos)
            echo_map[upper.substr(pos + 4)] = pin;
    }
    for (const auto& t : trig_map) {
        auto e_it = echo_map.find(t.first);
        if (e_it == echo_map.end()) continue;
        int trig_pin = t.second;
        int echo_pin = e_it->second;
        DetectedComponent comp;
        comp.type      = ComponentType::DistanceSensor;
        comp.pin       = echo_pin;
        comp.pins      = {trig_pin, echo_pin};
        comp.pin_name  = "HC-SR04";
        comp.label     = "Distance Sensor (TRIG=" + std::to_string(trig_pin)
                       + ", ECHO=" + std::to_string(echo_pin) + ")";
        comp.confirmed = false;
        components_.push_back(comp);
        claimed.insert(trig_pin);
        claimed.insert(echo_pin);
    }

    // --- H-bridge motors ---
    // Group defines by prefix: MOTOR1_, MOTOR2_, MOTOR3_
    // Pin values above 53 are treated as constants (e.g. PWM duty cycle), not pin numbers.
    static constexpr int MAX_PIN = 53;
    std::map<std::string, std::vector<std::pair<std::string, int>>> groups;
    for (const auto& d : defines) {
        std::string upper = to_upper(d.first);
        int pin = resolve_pin(d.second, defines);
        if (pin < 0 || pin > MAX_PIN) continue;
        if (contains_any(upper, {"MOTOR", "SRV", "SERVO"})) {
            size_t pos = upper.find('_');
            if (pos != std::string::npos) {
                std::string prefix = upper.substr(0, pos);
                groups[prefix].push_back({upper, pin});
            }
        }
    }

    for (const auto& g : groups) {
        const auto& pins = g.second;
        if (pins.size() >= 2) {
            // Sort into canonical order: pins[0]=PWM, pins[1]=CWISE, pins[2]=ANTI_CWISE
            int pwm_pin = -1, cwise_pin = -1, anti_cwise_pin = -1;
            for (const auto& p : pins) {
                const std::string& name = p.first;
                if (contains_any(name, {"PWM"}))
                    pwm_pin = p.second;
                else if (contains_any(name, {"ANTI"}))
                    anti_cwise_pin = p.second;
                else if (contains_any(name, {"CWISE", "CW", "DIR"}))
                    cwise_pin = p.second;
            }
            // Fall back to source order for any unrecognised suffixes
            if (pwm_pin < 0)       pwm_pin       = pins[0].second;
            if (cwise_pin < 0)     cwise_pin     = (pins.size() > 1) ? pins[1].second : -1;
            if (anti_cwise_pin < 0) anti_cwise_pin = (pins.size() > 2) ? pins[2].second : -1;

            DetectedComponent comp;
            comp.type     = ComponentType::HBridgeMotor;
            comp.pin      = pwm_pin;
            comp.pins     = { pwm_pin, cwise_pin, anti_cwise_pin };
            comp.pin_name = g.first;
            comp.label    = "Motor (" + g.first + ")";
            comp.confirmed = false;
            components_.push_back(comp);
            for (const auto& p : pins)
                claimed.insert(p.second);
        }
    }

    // --- Color Sensor ---
    // Look for S0, S1, S2, S3, sensorOut arrays all present and same length
    bool has_s0 = false, has_s1 = false, has_s2 = false, has_s3 = false, has_out = false;
    std::vector<int> s0_pins, s1_pins, s2_pins, s3_pins, out_pins;
    for (const auto& arr : arrays) {
        std::string upper = to_upper(arr.first);
        if (contains_any(upper, {"S0"})) { has_s0 = true; s0_pins = arr.second; }
        if (contains_any(upper, {"S1"})) { has_s1 = true; s1_pins = arr.second; }
        if (contains_any(upper, {"S2"})) { has_s2 = true; s2_pins = arr.second; }
        if (contains_any(upper, {"S3"})) { has_s3 = true; s3_pins = arr.second; }
        if (contains_any(upper, {"OUT", "SENSOROUT", "OUTPUT"})) { has_out = true; out_pins = arr.second; }
    }

    // If all 5 arrays are present and have the same number of pins, assume it's a color sensor
    if (has_s0 && has_s1 && has_s2 && has_s3 && has_out &&
        s0_pins.size() == s1_pins.size() &&
        s0_pins.size() == s2_pins.size() &&
        s0_pins.size() == s3_pins.size() &&
        s0_pins.size() == out_pins.size()) {
        for (size_t i = 0; i < s0_pins.size(); ++i) {
            DetectedComponent comp;
            comp.type      = ComponentType::ColorSensor;
            comp.pin       = out_pins[i]; // Representative pin
            comp.pins      = {s0_pins[i], s1_pins[i], s2_pins[i], s3_pins[i], out_pins[i]};
            comp.pin_name  = "ColorSensor";
            comp.label     = "Color Sensor (S0=" + std::to_string(s0_pins[i]) +
                             ", S1=" + std::to_string(s1_pins[i]) +
                             ", S2=" + std::to_string(s2_pins[i]) +
                             ", S3=" + std::to_string(s3_pins[i]) +
                             ", OUT=" + std::to_string(out_pins[i]) + ")";
            comp.confirmed = false;
            components_.push_back(comp);
            claimed.insert(s0_pins[i]);
            claimed.insert(s1_pins[i]);
            claimed.insert(s2_pins[i]);
            claimed.insert(s3_pins[i]);
            claimed.insert(out_pins[i]);
        }
    }

    // --- LCD 16x2 ---
    // Look for defines containing RS, E, D4, D5, D6, D7
    int rs_pin = -1, e_pin = -1, d4_pin = -1, d5_pin = -1, d6_pin = -1, d7_pin = -1;
    for (const auto& d : defines) {
        std::string upper = to_upper(d.first);
        int pin = resolve_pin(d.second, defines);
        if (pin < 0) continue;
        if (contains_any(upper, {"RS"})) rs_pin = pin;
        if (contains_any(upper, {"LCD_EN", "LCD_E", "_ENABLE", "_EN"})) e_pin = pin;
        if (contains_any(upper, {"D4"})) d4_pin = pin;
        if (contains_any(upper, {"D5"})) d5_pin = pin;
        if (contains_any(upper, {"D6"})) d6_pin = pin;
        if (contains_any(upper, {"D7"})) d7_pin = pin;
    }

    // If all 6 pins are found, assume it's an LCD 16x2
    if (rs_pin >= 0 && e_pin >= 0 && d4_pin >= 0 && d5_pin >= 0 && d6_pin >= 0 && d7_pin >= 0) {
        DetectedComponent comp;
        comp.type      = ComponentType::LCD;
        comp.pin       = rs_pin; // Representative pin
        comp.pins      = {rs_pin, e_pin, d4_pin, d5_pin, d6_pin, d7_pin};
        comp.pin_name  = "LCD";
        comp.label     = "LCD 16x2 (RS=" + std::to_string(rs_pin) +
                         ", E=" + std::to_string(e_pin) +
                         ", D4=" + std::to_string(d4_pin) +
                         ", D5=" + std::to_string(d5_pin) +
                         ", D6=" + std::to_string(d6_pin) +
                         ", D7=" + std::to_string(d7_pin) + ")";
        comp.confirmed = false;
        components_.push_back(comp);
        claimed.insert(rs_pin);
        claimed.insert(e_pin);
        claimed.insert(d4_pin);
        claimed.insert(d5_pin);
        claimed.insert(d6_pin);
        claimed.insert(d7_pin);
    }

    // --- Servo ---
    // Find "Servo Name;" declarations, then "Name.attach(pin)" calls
    {
        std::regex servo_decl_re(R"(\bServo\s+(\w+)\s*;)");
        auto sd_begin = std::sregex_iterator(source.begin(), source.end(), servo_decl_re);
        auto sd_end   = std::sregex_iterator();

        for (auto it = sd_begin; it != sd_end; ++it) {
            std::string servo_name = (*it)[1].str();

            std::regex attach_re("\\b" + servo_name + R"(\s*\.\s*attach\s*\(\s*(\w+)\s*\))");
            auto ab = std::sregex_iterator(source.begin(), source.end(), attach_re);
            auto ae = std::sregex_iterator();

            for (auto ait = ab; ait != ae; ++ait) {
                std::string pin_token = (*ait)[1].str();
                int pin = resolve_pin(pin_token, defines);
                if (pin < 0 || claimed.count(pin)) continue;

                DetectedComponent comp;
                comp.type     = ComponentType::Servo;
                comp.pin      = pin;
                comp.pins     = {pin};
                comp.pin_name = servo_name;
                comp.label    = "Servo " + servo_name + " (pin " + std::to_string(pin) + ")";
                comp.confirmed = false;
                components_.push_back(comp);
                claimed.insert(pin);
            }
        }
    }

    // Add more multi-pin component detection logic here as needed...

    return claimed;
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