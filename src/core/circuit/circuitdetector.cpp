#include "src/core/circuit/circuitdetector.h"
#include "src/core/circuit/componentregistry.h"
#include <cstddef>
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <map>
#include <string>

void CircuitDetector::detect(const std::string& source) {
    reset();

    auto defines  = parse_defines(source);
    auto arrays   = parse_arrays(source);                         
    auto claimed  = detect_multipin(source, defines, arrays);     
    auto pinmodes = parse_pinmodes(source, defines);

    for (const auto& pm : pinmodes) {
        if (claimed.count(pm.pin)) continue;
        DetectedComponent comp;
        comp.pin       = pm.pin;
        comp.pin_name  = pm.pin_name;
        comp.type_name = infer_type(pm.pin_name, pm.mode);
        comp.confirmed = false;

        std::string type_str = comp.type_name;

        comp.label = comp.pin >= 0
            ? type_str + " (pin " + std::to_string(comp.pin) + ")"
            : type_str;

        if (!pin_already_added(comp.pin)) {
            components_.push_back(comp);
        } else {
            for (const auto& existing : components_) {
                if (existing.pin == comp.pin) {
                    warnings_.push_back(
                        "WARNING: Pin " + std::to_string(comp.pin) +
                        " is used by both '" + existing.label + "' and '" + comp.label +
                        "' — only " + existing.label + " will be simulated");
                    break;
                }
            }
        }
    }

    // detect components from analogRead calls
    static const std::regex analog_re(R"((?:api->)?analogRead\s*\(\s*(\w+)\s*\))");
    auto ab = std::sregex_iterator(source.begin(), source.end(), analog_re);
    auto ae = std::sregex_iterator();

    for (auto it = ab; it != ae; ++it) {
        std::smatch m = *it;
        std::string token = m[1].str();
        int pin = resolve_pin(token, defines);
        if (pin < 0) continue;
        if (claimed.count(pin)) continue;

        if (pin_already_added(pin)) {
            for (const auto& existing : components_) {
                if (existing.pin == pin) {
                    std::string label = "Analog Sensor (pin " + std::to_string(pin) + ")";
                    warnings_.push_back(
                        "WARNING: Pin " + std::to_string(pin) +
                        " is used by both '" + existing.label + "' and '" + label +
                        "' — only " + existing.label + " will be simulated");
                    break;
                }
            }
            continue;
        }

        DetectedComponent comp;
        comp.pin       = pin;
        comp.pin_name  = defines.count(token) ? token : "";
        comp.type_name = infer_type(token, "INPUT");
        comp.confirmed = false;

        comp.label = comp.type_name + " (pin " + std::to_string(pin) + ")";

        components_.push_back(comp);
    }

    // Check for Serial.begin()
    if (source.find("Serial.begin") != std::string::npos ||
        source.find("Serial_begin") != std::string::npos) {
        DetectedComponent serial;
        serial.type_name = "Serial";
        serial.pin      = -1;
        serial.pin_name = "Serial";
        serial.label    = "Serial monitor";
        serial.confirmed = false;
        components_.push_back(serial);
    }

    // Post-process: append [n] index to labels when multiple components share the same type
    std::map<std::string, int> type_counts;
    for (const auto& c : components_)
        type_counts[c.type_name]++;

    std::map<std::string, int> type_idx;
    for (auto& c : components_) {
        if (type_counts[c.type_name] > 1) {
            int idx = type_idx[c.type_name]++;
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
    warnings_.clear();
}

// Phase 1 -- parse #defines and const int scalars into a symbol table
// Handles: #define NAME VALUE, #define NAME (VALUE), and const int NAME = VALUE;
std::map<std::string, std::string> CircuitDetector::parse_defines(
    const std::string& source)
{
    std::map<std::string, std::string> defines;

    // Match: #define IDENTIFIER value
    static const std::regex define_re(R"(#\s*define\s+(\w+)\s+\(?\s*(\w+)\s*\)?)");
    auto begin = std::sregex_iterator(source.begin(), source.end(), define_re);
    auto end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::smatch m = *it;
        defines[m[1].str()] = m[2].str();
    }

    // Also match: const int NAME = VALUE; (single value, not arrays)
    // This lets pin names like "const int trigPin1 = 9;" resolve just like #defines.
    static const std::regex const_int_re(R"(const\s+int\s+(\w+)\s*=\s*(\w+)\s*;)");
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
    static const std::regex pattern(R"(const\s+int\s+(\w+)\s*\[.*?\]\s*=\s*\{([^}]+)\})");
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

void CircuitDetector::add_multipin_component(
    const ComponentDefinition& def,
    const std::vector<int>& pins,
    const std::string& group_label,
    std::set<int>& claimed)
{
    size_t rep_idx = 0;
    if (!def.representative_role.empty()) {
        for (size_t i = 0; i < def.detect_multi.size(); ++i) {
            if (def.detect_multi[i].name == def.representative_role) { rep_idx = i; break; }
        }
    }
    if (rep_idx >= pins.size() || pins[rep_idx] < 0) return;
    int rep_pin = pins[rep_idx];

    DetectedComponent comp;
    comp.type_name = def.type_name;
    comp.pin       = rep_pin;
    comp.pins      = pins;
    comp.pin_name  = group_label.empty() ? def.type_name : group_label;
    comp.confirmed = false;

    std::string label = def.type_name + " (";
    bool first = true;
    for (size_t i = 0; i < def.detect_multi.size() && i < pins.size(); ++i) {
        if (pins[i] < 0) continue;
        if (!first) label += ", ";
        first = false;
        label += def.detect_multi[i].name + "=" + std::to_string(pins[i]);
    }
    label += ")";
    comp.label = label;

    if (pin_already_added(rep_pin)) {
        for (const auto& existing : components_) {
            if (existing.pin == rep_pin) {
                warnings_.push_back("WARNING: Pin " + std::to_string(rep_pin) +
                    " is used by both '" + existing.label + "' and '" + comp.label +
                    "' — only " + existing.label + " will be simulated");
                break;
            }
        }
        return;
    }

    components_.push_back(comp);
    for (int p : pins)
        if (p >= 0) claimed.insert(p);
}

void CircuitDetector::detect_suffix_group(
    const ComponentDefinition& def,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    std::vector<std::map<std::string, int>> role_suffix(def.detect_multi.size());

    for (const auto& d : defines) {
        std::string upper = to_upper(d.first);
        int pin = resolve_pin(d.second, defines);
        if (pin < 0) continue;
        for (size_t r = 0; r < def.detect_multi.size(); ++r) {
            bool matched = false;
            for (const auto& kw : def.detect_multi[r].keywords) {
                size_t pos = upper.find(kw);
                if (pos != std::string::npos) {
                    role_suffix[r][upper.substr(pos + kw.size())] = pin;
                    matched = true;
                    break;
                }
            }
            if (matched) break;
        }
    }

    if (role_suffix.empty()) return;
    for (const auto& [suffix, pin0] : role_suffix[0]) {
        std::vector<int> pins;
        bool all_found = true;
        for (auto& rs : role_suffix) {
            auto it = rs.find(suffix);
            if (it == rs.end()) { all_found = false; break; }
            pins.push_back(it->second);
        }
        if (!all_found) continue;
        add_multipin_component(def, pins, def.type_name + suffix, claimed);
    }
}

void CircuitDetector::detect_prefix_group(
    const ComponentDefinition& def,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    static constexpr int MAX_PIN = 53;
    std::vector<std::string> all_keywords;
    for (const auto& role : def.detect_multi)
        for (const auto& kw : role.keywords)
            all_keywords.push_back(kw);

    std::map<std::string, std::vector<std::pair<std::string, int>>> groups;
    for (const auto& d : defines) {
        std::string upper = to_upper(d.first);
        int pin = resolve_pin(d.second, defines);
        if (pin < 0 || pin > MAX_PIN) continue;
        if (!contains_any(upper, all_keywords)) continue;
        size_t pos = upper.find('_');
        if (pos == std::string::npos) continue;
        groups[upper.substr(0, pos)].push_back({upper, pin});
    }

    for (const auto& g : groups) {
        const auto& members = g.second;
        if (members.size() < 2) continue;

        // Every role here has keywords that distinguish it from its siblings
        // (PWM/ANTI/CWISE etc.) -- only assign a role its keyword-matched
        // pin. A leftover-member fallback used to fill any still-unfilled
        // role with whatever unused member came next in map order, which
        // could wire an unrelated pin sharing the same prefix (e.g. an extra
        // enable pin) into a role it was never named for.
        std::vector<int> pins(def.detect_multi.size(), -1);
        std::vector<bool> used(members.size(), false);
        for (size_t r = 0; r < def.detect_multi.size(); ++r) {
            for (size_t m = 0; m < members.size(); ++m) {
                if (used[m]) continue;
                if (contains_any(members[m].first, def.detect_multi[r].keywords)) {
                    pins[r] = members[m].second;
                    used[m] = true;
                    break;
                }
            }
        }

        int filled = 0;
        for (int p : pins) if (p >= 0) filled++;
        if (filled < 2) continue;

        add_multipin_component(def, pins, g.first, claimed);
    }
}

void CircuitDetector::detect_array_group(
    const ComponentDefinition& def,
    const std::map<std::string, std::vector<int>>& arrays,
    std::set<int>& claimed)
{
    std::vector<std::vector<int>> role_pins(def.detect_multi.size());
    std::vector<bool> role_found(def.detect_multi.size(), false);

    for (const auto& arr : arrays) {
        std::string upper = to_upper(arr.first);
        for (size_t r = 0; r < def.detect_multi.size(); ++r) {
            if (role_found[r]) continue;
            if (contains_any(upper, def.detect_multi[r].keywords)) {
                role_pins[r] = arr.second;
                role_found[r] = true;
                break;
            }
        }
    }

    for (bool f : role_found) if (!f) return;

    size_t len = role_pins[0].size();
    for (const auto& rp : role_pins) if (rp.size() != len) return;

    for (size_t i = 0; i < len; ++i) {
        std::vector<int> pins;
        for (const auto& rp : role_pins) pins.push_back(rp[i]);
        add_multipin_component(def, pins, def.type_name, claimed);
    }
}

void CircuitDetector::detect_singleton_group(
    const ComponentDefinition& def,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    std::vector<int> pins(def.detect_multi.size(), -1);
    for (const auto& d : defines) {
        std::string upper = to_upper(d.first);
        int pin = resolve_pin(d.second, defines);
        if (pin < 0 || claimed.count(pin)) continue;
        for (size_t r = 0; r < def.detect_multi.size(); ++r) {
            if (pins[r] >= 0) continue;
            if (contains_any(upper, def.detect_multi[r].keywords)) {
                pins[r] = pin;
                break;
            }
        }
    }
    for (int p : pins) if (p < 0) return;
    add_multipin_component(def, pins, def.type_name, claimed);
}

void CircuitDetector::detect_generic_multipin(
    const std::map<std::string, std::string>& defines,
    const std::map<std::string, std::vector<int>>& arrays,
    std::set<int>& claimed)
{
    for (const auto& def : ComponentRegistry::instance().all()) {
        switch (def.multi_pin_strategy) {
            case MultiPinStrategy::Suffix:    detect_suffix_group(def, defines, claimed); break;
            case MultiPinStrategy::Prefix:    detect_prefix_group(def, defines, claimed); break;
            case MultiPinStrategy::Array:     detect_array_group(def, arrays, claimed); break;
            case MultiPinStrategy::Singleton: detect_singleton_group(def, defines, claimed); break;
            case MultiPinStrategy::None:      break;
        }
    }
}

void CircuitDetector::detect_keypad_matrix(
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    const std::map<std::string, std::vector<int>>& arrays,
    std::set<int>& claimed)
{
    (void)arrays; // rowPins[]/colPins[] use byte/int, not the const-int-only shared array parser
    static constexpr int MIN_LINES = 2, MAX_LINES = 4;

    // Require actual keypad usage, not just any ROW/COL-named pin, so this
    // never fires on an unrelated matrix/grid sketch.
    if (source.find("Keypad") == std::string::npos) return;
    if (!ComponentRegistry::instance().find_by_type("Keypad")) return;

    auto has_role_keyword = [](const std::string& upper, const char* kw) {
        return upper.find(kw) != std::string::npos;
    };

    // Preferred: rowPins[]/colPins[] arrays of independent length -- the shape
    // every real Keypad.h tutorial sketch declares (byte rowPins[ROWS] = {...}).
    auto scan_pin_array = [&](const char* keyword) -> std::vector<int> {
        static const std::regex arr_re(
            R"((?:const\s+)?(?:byte|int|uint8_t)\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{([^}]+)\})");
        for (auto it = std::sregex_iterator(source.begin(), source.end(), arr_re);
             it != std::sregex_iterator(); ++it) {
            std::string name = (*it)[1].str();
            if (!has_role_keyword(to_upper(name), keyword)) continue;
            std::vector<int> pins;
            std::stringstream ss((*it)[2].str());
            std::string token;
            while (std::getline(ss, token, ',')) {
                token.erase(0, token.find_first_not_of(" \t\r\n"));
                token.erase(token.find_last_not_of(" \t\r\n") + 1);
                int pin = resolve_pin(token, defines);
                if (pin < 0) return {};
                pins.push_back(pin);
            }
            return pins;
        }
        return {};
    };

    std::vector<int> row_pins = scan_pin_array("ROW");
    std::vector<int> col_pins = scan_pin_array("COL");

    // Fallback: grouped #defines -- #define ROW1 9 / ROW2 8 ... (same for COL).
    auto scan_defines = [&](const char* keyword) -> std::vector<int> {
        static const std::regex num_re(R"((\d+)$)");
        std::vector<std::pair<int,int>> numbered; // (suffix number, pin)
        for (const auto& d : defines) {
            std::string upper = to_upper(d.first);
            if (!has_role_keyword(upper, keyword)) continue;
            std::smatch m;
            if (!std::regex_search(upper, m, num_re)) continue;
            int pin = resolve_pin(d.second, defines);
            if (pin < 0) continue;
            numbered.push_back({std::stoi(m[1].str()), pin});
        }
        std::sort(numbered.begin(), numbered.end());
        std::vector<int> pins;
        for (auto& np : numbered) pins.push_back(np.second);
        return pins;
    };

    if (row_pins.empty()) row_pins = scan_defines("ROW");
    if (col_pins.empty()) col_pins = scan_defines("COL");

    int rows = (int)row_pins.size();
    int cols = (int)col_pins.size();
    if (rows < MIN_LINES || rows > MAX_LINES || cols < MIN_LINES || cols > MAX_LINES) return;

    for (int p : row_pins) if (claimed.count(p)) return;
    for (int p : col_pins) if (claimed.count(p)) return;

    DetectedComponent comp;
    comp.type_name = "Keypad";
    comp.pin       = row_pins[0];
    comp.pins      = row_pins;
    comp.pins.insert(comp.pins.end(), col_pins.begin(), col_pins.end());
    comp.rows      = rows;
    comp.cols      = cols;
    comp.pin_name  = "Keypad";
    comp.confirmed = false;

    std::string label = "Keypad (ROW=";
    for (size_t i = 0; i < row_pins.size(); ++i) label += (i ? "," : "") + std::to_string(row_pins[i]);
    label += " COL=";
    for (size_t i = 0; i < col_pins.size(); ++i) label += (i ? "," : "") + std::to_string(col_pins[i]);
    label += ")";
    comp.label = label;

    if (pin_already_added(comp.pin)) return;

    components_.push_back(comp);
    for (int p : comp.pins) claimed.insert(p);
}

void CircuitDetector::detect_dht(
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    if (!ComponentRegistry::instance().find_by_type("DHT")) return;

    // "DHT dht(DHTPIN, DHTTYPE)" -- DHTTYPE only needs to resolve for the
    // display label, not for placement, so it isn't required to be a pin.
    static const std::regex ctor_re(
        R"(\bDHT\s+(\w+)\s*\(\s*(\w+)\s*(?:,\s*(\w+)\s*)?\))");
    auto it = std::sregex_iterator(source.begin(), source.end(), ctor_re);
    auto end_it = std::sregex_iterator();
    if (it == end_it) return;

    std::string obj_name  = (*it)[1].str();
    std::string pin_token = (*it)[2].str();
    std::string type_token = it->size() > 3 ? (*it)[3].str() : "";

    int pin = resolve_pin(pin_token, defines);
    if (pin < 0 || claimed.count(pin) || pin_already_added(pin)) return;

    std::string type_label;
    auto type_it = defines.find(type_token);
    if (type_it != defines.end()) type_label = type_it->second;

    DetectedComponent comp;
    comp.type_name = "DHT";
    comp.pin       = pin;
    comp.pins      = {pin};
    comp.pin_name  = obj_name;
    comp.confirmed = false;
    comp.label = "DHT" + (type_label.empty() ? "" : " " + type_label) +
                 " " + obj_name + " (pin " + std::to_string(pin) + ")";

    components_.push_back(comp);
    claimed.insert(pin);
}

void CircuitDetector::detect_max7219(
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    if (!ComponentRegistry::instance().find_by_type("Max7219")) return;

    // Matches both "LedControl lc(a, b, c[, d]);" and
    // "LedControl lc = LedControl(a, b, c[, d]);" -- real usage almost always
    // takes the assignment form. Args are positional per the real LedControl
    // API: (dataPin, clkPin, csPin, numDevices).
    static const std::regex ctor_re(
        R"(\bLedControl\s+(\w+)\s*(?:=\s*LedControl\s*)?\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*(?:,\s*(\w+)\s*)?\))");

    for (auto it = std::sregex_iterator(source.begin(), source.end(), ctor_re);
         it != std::sregex_iterator(); ++it) {
        std::string obj_name = (*it)[1].str();
        int dinPin = resolve_pin((*it)[2].str(), defines);
        int clkPin = resolve_pin((*it)[3].str(), defines);
        int csPin  = resolve_pin((*it)[4].str(), defines);
        if (dinPin < 0 || clkPin < 0 || csPin < 0) continue;
        if (claimed.count(csPin) || pin_already_added(csPin)) continue;

        int num_devices = 1;
        if (it->size() > 5 && (*it)[5].matched) {
            std::string tok = (*it)[5].str();
            try {
                num_devices = std::stoi(tok);
            } catch (...) {
                auto dit = defines.find(tok);
                if (dit != defines.end()) {
                    try { num_devices = std::stoi(dit->second); } catch (...) {}
                }
            }
        }
        num_devices = std::max(1, std::min(8, num_devices));

        DetectedComponent comp;
        comp.type_name   = "Max7219";
        comp.pin         = csPin;
        comp.pins        = {csPin, clkPin, dinPin};
        comp.pin_name    = obj_name;
        comp.confirmed   = false;
        comp.num_devices = num_devices;
        comp.label = "Max7219 " + obj_name + " (CS=" + std::to_string(csPin) +
                     ", CLK=" + std::to_string(clkPin) + ", DIN=" + std::to_string(dinPin) +
                     (num_devices > 1 ? ", devices=" + std::to_string(num_devices) : "") + ")";

        components_.push_back(comp);
        claimed.insert(csPin);
        claimed.insert(clkPin);
        claimed.insert(dinPin);
    }
}

std::string CircuitDetector::regex_escape(const std::string& s) {
    static const std::string special = ".()[]{}+*?^$|\\";
    std::string out;
    for (char c : s) {
        if (special.find(c) != std::string::npos) out += '\\';
        out += c;
    }
    return out;
}

void CircuitDetector::detect_method_call_pattern(
    const ComponentDefinition& def,
    const std::string& pattern,
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    std::regex call_re(R"(\b(\w+)\s*)" + regex_escape(pattern) + R"(\s*(\w+)\s*\))");
    for (auto it = std::sregex_iterator(source.begin(), source.end(), call_re);
         it != std::sregex_iterator(); ++it) {
        std::string obj_name  = (*it)[1].str();
        std::string pin_token = (*it)[2].str();
        int pin = resolve_pin(pin_token, defines);
        if (pin < 0 || claimed.count(pin) || pin_already_added(pin)) continue;

        DetectedComponent comp;
        comp.type_name = def.type_name;
        comp.pin       = pin;
        comp.pins      = {pin};
        comp.pin_name  = obj_name;
        comp.label     = def.type_name + " " + obj_name + " (pin " + std::to_string(pin) + ")";
        comp.confirmed = false;
        components_.push_back(comp);
        claimed.insert(pin);
    }
}

void CircuitDetector::detect_wrapper_function_pattern(
    const ComponentDefinition& def,
    const std::string& pattern,
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    std::regex pattern_re(R"(\b)" + regex_escape(pattern));
    static const std::regex func_def_re(R"(\b(?:int|long|void|float)\s+(\w+)\s*\([^)]*\)\s*\{)");

    std::set<std::string> wrapper_funcs;
    for (auto it = std::sregex_iterator(source.begin(), source.end(), pattern_re);
         it != std::sregex_iterator(); ++it) {
        std::string before = source.substr(0, it->position());
        std::string last_func;
        for (auto fit = std::sregex_iterator(before.begin(), before.end(), func_def_re);
             fit != std::sregex_iterator(); ++fit)
            last_func = (*fit)[1].str();
        if (!last_func.empty())
            wrapper_funcs.insert(last_func);
    }

    for (const auto& fn : wrapper_funcs) {
        std::regex call_re("\\b" + fn + R"(\s*\(\s*(\w+)\s*\))");
        for (auto it = std::sregex_iterator(source.begin(), source.end(), call_re);
             it != std::sregex_iterator(); ++it) {
            std::string pin_token = (*it)[1].str();
            int pin = resolve_pin(pin_token, defines);
            if (pin < 0 || claimed.count(pin) || pin_already_added(pin)) continue;

            DetectedComponent comp;
            comp.type_name = def.type_name;
            comp.pin       = pin;
            comp.pins      = {pin};
            comp.pin_name  = pin_token;
            comp.label     = def.type_name + " (pin " + std::to_string(pin) + ")";
            comp.confirmed = false;
            components_.push_back(comp);
            claimed.insert(pin);
        }
    }
}

void CircuitDetector::detect_constructor_pattern(
    const ComponentDefinition& def,
    const std::string& pattern,
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    if (def.detect_multi.empty()) return;
    for (const auto& c : components_)
        if (c.type_name == def.type_name) return;

    size_t n = def.detect_multi.size();
    std::string regex_str = regex_escape(pattern) + R"(\s+\w+\s*\()";
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) regex_str += ",";
        regex_str += R"(\s*(\w+)\s*)";
    }
    regex_str += R"(\))";

    std::smatch m;
    std::regex ctor_re(regex_str);
    if (!std::regex_search(source, m, ctor_re)) return;

    auto resolve_arg = [&](const std::string& arg) -> int {
        try { return std::stoi(arg); } catch (...) {}
        return resolve_pin(arg, defines);
    };

    std::vector<int> pins;
    for (size_t i = 0; i < n; ++i)
        pins.push_back(resolve_arg(m[i + 1].str()));
    for (int p : pins)
        if (p < 0) return;

    add_multipin_component(def, pins, def.type_name, claimed);
}

void CircuitDetector::detect_pattern_matches(
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    std::set<int>& claimed)
{
    for (const auto& def : ComponentRegistry::instance().all()) {
        for (const auto& pattern : def.detect_pattern) {
            if (pattern.empty()) continue;
            if (pattern.front() == '.' && pattern.back() == '(')
                detect_method_call_pattern(def, pattern, source, defines, claimed);
            else if (pattern.back() == '(')
                detect_wrapper_function_pattern(def, pattern, source, defines, claimed);
            else
                detect_constructor_pattern(def, pattern, source, defines, claimed);
        }
    }
}

std::set<int> CircuitDetector::detect_multipin(
    const std::string& source,
    const std::map<std::string, std::string>& defines,
    const std::map<std::string, std::vector<int>>& arrays)
{
    std::set<int> claimed;

    detect_generic_multipin(defines, arrays, claimed);
    detect_pattern_matches(source, defines, claimed);
    detect_keypad_matrix(source, defines, arrays, claimed);
    detect_dht(source, defines, claimed);
    detect_max7219(source, defines, claimed);

    return claimed;
}


// Phase 2 -- find pinMode(pin, mode) calls
std::vector<CircuitDetector::PinModeCall> CircuitDetector::parse_pinmodes(
    const std::string& source,
    const std::map<std::string, std::string>& defines)
{
    std::vector<PinModeCall> calls;

    // Match: pinMode(token, MODE) or api->pinMode(token, MODE)
    static const std::regex pattern(
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
std::string CircuitDetector::infer_type(
    const std::string& name, const std::string& mode)
{
    std::string upper = to_upper(name);

    const ComponentDefinition* def = ComponentRegistry::instance().find_by_single_keyword(upper);
    if (def) return def->type_name;

    if (mode == "OUTPUT")
        return "GenericOutput";

    return "GenericInput";
}

int CircuitDetector::resolve_pin(
    const std::string& token,
    const std::map<std::string, std::string>& defines,
    int depth)
{
    // Direct number
    if (!token.empty() && std::isdigit(token[0]))
        return std::stoi(token);

    // Arduino analog pin A0-A7 = 14-21
    if (token.size() >= 2 && token[0] == 'A' && std::isdigit(token[1]))
        return 14 + (token[1] - '0');

    // Guard against self-referential #define chains (e.g. "#define A B" /
    // "#define B A") recursing forever and stack-overflowing the app -- a
    // valid chain can pass through at most defines.size() distinct keys
    // before it must terminate or repeat.
    if (depth > (int)defines.size())
        return -1;

    // Look up in defines
    auto it = defines.find(token);
    if (it != defines.end())
        return resolve_pin(it->second, defines, depth + 1); // recurse in case of chained defines

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

bool CircuitDetector::pin_already_added(int pin) const {
    for (const auto& c : components_)
        if (c.pin == pin) return true;
    return false;
}

std::string DetectedComponent::to_string() const {
    // pull the pin or pins, pin_name, and label
    std::string result;

    if (!pin_name.empty()){
        result += "- Pin Name: " + pin_name + "\n";
    }

    if (!pins.empty()){
        result += "- Pins: ";
        for (const auto& p:pins) {
            result += std::to_string(p) + ", ";
        }
        result += "\n";
    } else {
        result += "- Pin: ";
        result += std::to_string(pin) + "\n";
    }

    if (!label.empty()) {
        result += "- Label: " + label + "\n";
    }

    return result;
}