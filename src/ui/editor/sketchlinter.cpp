#include "src/ui/editor/sketchlinter.h"
#include <regex>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <QRegularExpression>

namespace {

// PWM-capable pins per board; empty = board supports PWM on all pins (skip check)
std::vector<int> get_pwm_pins_for(const BoardProfile& p) {
    std::string name = p.name;
    if (name == "Arduino Uno" || name == "Arduino Nano")
        return {3, 5, 6, 9, 10, 11};
    if (name == "Arduino Mega 2560")
        return {2,3,4,5,6,7,8,9,10,11,12,13,44,45,46};
    return {}; // Due / Teensy: all pins support PWM
}

} // namespace

namespace SketchLinter {

bool isPinName(const std::string& name) {
    static const char* INCLUDE[] = {
        "PIN", "LED", "BTN", "BUTTON", "SERVO", "TRIG", "ECHO",
        "SENSOR", "MOTOR", "CLK", "DT", "CS", "RST", "RS",
        "SWITCH", "BUZZER", "SPEAKER", "PIEZO", "POT", "DIAL"
    };
    static const char* EXCLUDE[] = {
        "COUNT", "NUM", "SIZE", "LEN", "MAX", "SPEED", "BAUD", "RATE", "DELAY", "TIMEOUT"
    };
    std::string up = name;
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    for (const char* kw : EXCLUDE)
        if (up.find(kw) != std::string::npos) return false;
    for (const char* kw : INCLUDE)
        if (up.find(kw) != std::string::npos) return true;
    return false;
}

QString humanizeErrors(const QString& raw) {
    struct Rule { QRegularExpression re; QString repl; };
    static const Rule rules[] = {
        { QRegularExpression(R"('([\w:<>*& ]+)' was not declared in this scope)"),
          "'\\1' not found — did you forget to declare it?" },
        { QRegularExpression(R"(no matching function for call to '([^']+)')"),
          "Wrong arguments passed to '\\1'" },
        { QRegularExpression(R"(expected ';' before '}' token)"),
          "Missing semicolon, probably the line above" },
        { QRegularExpression(R"(expected '}' at end of input)"),
          "Unclosed brace — one of your { was never closed" },
        { QRegularExpression(R"(lvalue required as left operand of assignment)"),
          "Did you mean == instead of =?" },
        { QRegularExpression(R"(undefined reference to [`']([^'`]+)[`'])"),
          "Function '\\1' is used but never defined" },
        { QRegularExpression(R"(control reaches end of non-void function)"),
          "Function is missing a return statement" },
        { QRegularExpression(R"(expected unqualified-id before '\{' token)"),
          "Code found outside a function — all code must be inside setup(), loop(), or another function" },
        { QRegularExpression(R"(too (many|few) arguments to function '([^']+)')"),
          "Wrong number of arguments passed to '\\2'" },
        { QRegularExpression(R"(stray '\\[^']*' in program)"),
          "Invalid character in code — this sometimes happens when copy-pasting from a website; try retyping the line" },
        { QRegularExpression(R"(overflow in implicit constant conversion)"),
          "Number is too large for this variable type — try using long instead of int" },
        { QRegularExpression(R"(comparison between pointer and integer)"),
          "Can't compare strings with == — use strcmp() or the String class" },
    };
    QString result = raw;
    for (const auto& r : rules)
        result.replace(r.re, r.repl);
    return result;
}

QStringList checkSource(const QString& source, const BoardProfile& board) {
    QStringList warnings;
    std::string src = source.toStdString();

    // Build pin symbol table from #define NAME VALUE and const int NAME = VALUE
    std::map<std::string, int> pin_defs;
    {
        auto scan = [&](const std::regex& re) {
            for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
                 it != std::sregex_iterator(); ++it) {
                std::string name = (*it)[1].str();
                try { pin_defs[name] = std::stoi((*it)[2].str()); } catch (...) {}
            }
        };
        scan(std::regex(R"(#\s*define\s+(\w+)\s+\(?\s*(\d+)\s*\)?)"));
        scan(std::regex(R"(const\s+int\s+(\w+)\s*=\s*(\d+)\s*;)"));
    }

    // Shared helper — extract any named void function's body by brace-counting
    auto extract_func_body = [&](const std::string& func_name) -> std::string {
        std::regex func_re("\\bvoid\\s+" + func_name + R"(\s*\(\s*\)\s*\{)");
        std::smatch m;
        std::string s = src;
        if (!std::regex_search(s, m, func_re)) return "";
        size_t pos = static_cast<size_t>(m.position()) + m.length() - 1;
        int depth = 1;
        size_t start = pos + 1;
        while (pos < src.size() - 1 && depth > 0) {
            ++pos;
            if (src[pos] == '{') ++depth;
            else if (src[pos] == '}') --depth;
        }
        return src.substr(start, pos - start);
    };

    // Pre-extract loop and setup bodies for reuse across checks
    std::string loop_body  = extract_func_body("loop");
    std::string setup_body = extract_func_body("setup");

    // Pin out of range for selected board
    for (const auto& [name, pin] : pin_defs) {
        if (!isPinName(name)) continue;
        if (pin < 0 || pin >= 80) continue;
        if (pin >= board.pin_count) {
            warnings << QString("WARNING: Pin %1 ('%2') is not available on the %3 (max pin %4)")
                .arg(pin)
                .arg(QString::fromStdString(name))
                .arg(board.name)
                .arg(board.pin_count - 1);
        }
    }

    // analogWrite() on a non-PWM pin
    auto pwm_pins = get_pwm_pins_for(board);
    if (!pwm_pins.empty()) {
        std::regex aw_re(R"((?:api->)?analogWrite\s*\(\s*(\w+))");
        std::set<int> warned_pins;
        for (auto it = std::sregex_iterator(src.begin(), src.end(), aw_re);
             it != std::sregex_iterator(); ++it) {
            std::string token = (*it)[1].str();
            int pin = -1;
            try { pin = std::stoi(token); } catch (...) {
                auto def_it = pin_defs.find(token);
                if (def_it != pin_defs.end()) pin = def_it->second;
            }
            if (pin < 0 || warned_pins.count(pin)) continue;
            if (std::find(pwm_pins.begin(), pwm_pins.end(), pin) == pwm_pins.end()) {
                warned_pins.insert(pin);
                warnings << QString("WARNING: Pin %1 does not support PWM on the %2 — analogWrite() will have no effect")
                    .arg(pin).arg(board.name);
            }
        }
    }

    // attachInterrupt() with raw interrupt number (Uno/Nano: interrupt 0=pin 2, 1=pin 3)
    {
        std::string boardName = board.name;
        if (boardName == "Arduino Uno" || boardName == "Arduino Nano") {
            std::regex ai_re(R"(attachInterrupt\s*\(\s*([01])\s*,)");
            std::set<std::string> warned_nums;
            for (auto it = std::sregex_iterator(src.begin(), src.end(), ai_re);
                 it != std::sregex_iterator(); ++it) {
                std::string n = (*it)[1].str();
                if (warned_nums.count(n)) continue;
                warned_nums.insert(n);
                int correct_pin = (n == "0") ? 2 : 3;
                warnings << QString("WARNING: attachInterrupt(%1, ...) uses an interrupt number, not a pin number — use digitalPinToInterrupt(%2) to attach to pin %2 on the %3")
                    .arg(QString::fromStdString(n)).arg(correct_pin).arg(board.name);
            }
        }
    }

    // delay() inside an attachInterrupt callback function
    {
        std::regex ai_re(R"(attachInterrupt\s*\([^,]+,\s*(\w+)\s*,)");
        std::set<std::string> warned_funcs;
        for (auto it = std::sregex_iterator(src.begin(), src.end(), ai_re);
             it != std::sregex_iterator(); ++it) {
            std::string fn = (*it)[1].str();
            if (warned_funcs.count(fn)) continue;
            std::string body = extract_func_body(fn);
            if (!body.empty() && body.find("delay(") != std::string::npos) {
                warned_funcs.insert(fn);
                warnings << QString("WARNING: delay() inside '%1' (used as an interrupt handler) will hang on real Arduino — interrupts are disabled during ISR execution")
                    .arg(QString::fromStdString(fn));
            }
        }
    }

    // Pin defined as an expression (CircuitDetector cannot evaluate it)
    {
        std::regex def_re(R"(#\s*define\s+(\w+)\s+(.+))");
        for (auto it = std::sregex_iterator(src.begin(), src.end(), def_re);
             it != std::sregex_iterator(); ++it) {
            std::string name  = (*it)[1].str();
            std::string value = (*it)[2].str();
            auto comment = value.find("//");
            if (comment != std::string::npos) value = value.substr(0, comment);
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
                value.pop_back();
            bool has_op = value.find('+') != std::string::npos ||
                          value.find('*') != std::string::npos ||
                          value.find('/') != std::string::npos ||
                          value.find("<<") != std::string::npos;
            if (!has_op && value.size() > 1 && value.find('-') != std::string::npos)
                has_op = (value.find('-') != 0 &&
                          !(value.find('-') == 1 && value[0] == '('));
            if (!has_op) continue;
            if (!isPinName(name)) continue;
            warnings << QString("WARNING: Pin '%1' is defined as an expression — the simulator could not evaluate it and the component may not appear on the canvas; use a plain number instead")
                .arg(QString::fromStdString(name));
        }
    }

    // pinMode() never called for a digitalWrite() pin
    {
        std::set<int> dw_pins;
        std::set<int> pm_pins;
        std::regex dw_re(R"(digitalWrite\s*\(\s*(\w+))");
        std::regex pm_re(R"(pinMode\s*\(\s*(\w+))");
        auto collect_pins = [&](const std::regex& re, std::set<int>& out) {
            for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
                 it != std::sregex_iterator(); ++it) {
                std::string token = (*it)[1].str();
                int pin = -1;
                try { pin = std::stoi(token); } catch (...) {
                    auto def_it = pin_defs.find(token);
                    if (def_it != pin_defs.end()) pin = def_it->second;
                }
                if (pin >= 0) out.insert(pin);
            }
        };
        collect_pins(dw_re, dw_pins);
        collect_pins(pm_re, pm_pins);
        for (int pin : dw_pins) {
            if (!pm_pins.count(pin)) {
                warnings << QString("WARNING: Pin %1 is used with digitalWrite() but pinMode() was never called — it will default to INPUT on real hardware")
                    .arg(pin);
            }
        }
    }

    // Missing volatile on ISR-shared variables
    {
        // Find all variables written inside attachInterrupt callbacks
        std::set<std::string> isr_written;
        std::regex ai_re(R"(attachInterrupt\s*\([^,]+,\s*(\w+)\s*,)");
        for (auto it = std::sregex_iterator(src.begin(), src.end(), ai_re);
             it != std::sregex_iterator(); ++it) {
            std::string fn = (*it)[1].str();
            std::string body = extract_func_body(fn);
            if (body.empty()) continue;
            // Find assignments in ISR body: varName = or varName++/varName--
            std::regex assign_re(R"(\b(\w+)\s*(?:=|\+=|-=|\+\+|--))" );
            for (auto jt = std::sregex_iterator(body.begin(), body.end(), assign_re);
                 jt != std::sregex_iterator(); ++jt) {
                isr_written.insert((*jt)[1].str());
            }
        }
        // Check if any ISR-written variables appear in loop/setup without volatile
        for (const auto& var : isr_written) {
            if (var.empty()) continue;
            if (var.size() < 3) continue; // skip short names like i, n, x
            // Check loop and setup bodies for reads of this variable
            bool read_in_main = (loop_body.find(var) != std::string::npos ||
                                 setup_body.find(var) != std::string::npos);
            if (!read_in_main) continue;
            // Check if declared volatile anywhere in source
            std::regex vol_re(R"(\bvolatile\b[^;]*\b)" + var + R"(\b)");
            if (!std::regex_search(src, vol_re)) {
                warnings << QString("WARNING: '%1' is shared with an ISR but not declared volatile — this may work in simulation but will likely fail on real hardware")
                    .arg(QString::fromStdString(var));
            }
        }
    }

    // String += in a tight loop
    {
        if (!loop_body.empty()) {
            std::regex str_concat_re(R"(\bString\b[^;]*\+=)");
            if (std::regex_search(loop_body, str_concat_re)) {
                warnings << QString("WARNING: Repeated String concatenation in loop() causes heap fragmentation on real Arduino — consider using a char buffer instead");
            }
        }
    }

    return warnings;
}

QStringList scanSymbols(const QString& source) {
    QStringList symbols;
    std::string src = source.toStdString();

    auto collect = [&](const std::regex& re) {
        for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
            it != std::sregex_iterator(); ++it) {
            QString name = QString::fromStdString((*it)[1].str());
            if (!symbols.contains(name))
                symbols << name;
        }
    };

    collect(std::regex(R"(#\s*define\s+(\w+))"));                                   // constants
    collect(std::regex(R"(\b(?:void|int|float|double|bool|long|char|byte|String)\s+(\w+)\s*\()"));  // functions
    collect(std::regex(R"(\b(?:int|float|double|bool|long|char|byte|String)\s+(\w+)\s*[=;])"));      // variables

    return symbols;
}

QMap<QString, QString> scanDeclaredTypes(const QString& source) {
    QMap<QString, QString> types;
    std::string src = source.toStdString();

    auto collect = [&](const std::regex& re, const QString& type_name) {
        for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
            it != std::sregex_iterator(); ++it) {
            types[QString::fromStdString((*it)[1].str())] = type_name;
        }
    };

    collect(std::regex(R"(\bServo\s+(\w+)\s*;)"), "Servo");
    collect(std::regex(R"(\bLiquidCrystal\s+(\w+)\s*\()"), "LiquidCrystal");
    collect(std::regex(R"(\bSoftwareSerial\s+(\w+)\s*\()"), "SoftwareSerial");

    return types;
}

} // namespace SketchLinter
