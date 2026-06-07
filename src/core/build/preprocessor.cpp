#include "src/core/build/preprocessor.h"
#include <cstring>
#include <regex>
#include <sstream>

std::string Preprocessor::process(const std::string& source) {
    // If already in VirtualEmbeddedProgrammer format, pass through unchanged
    if (is_already_transformed(source))
        return source;

    std::string result = source;
    // replace_api_calls must run before strip_includes so that API calls injected
    // by the Servo/library replacements are not double-prefixed with api->
    result = replace_api_calls(result);
    result = strip_includes(result);

    // Generate forward declarations before wrap_functions renames setup/loop
    std::string fwd = generate_forward_declarations(result);
    int fwd_lines = 0;
    if (!fwd.empty()) {
        fwd_lines = (int)std::count(fwd.begin(), fwd.end(), '\n') + 1; // +1 for blank separator
        result = fwd + "\n" + result;
    }

    result = wrap_functions(result);
    result = inject_safety_delay(result);
    result = inject_header(result);

    int header_lines = (int)std::count(g_injected_header,
                                        g_injected_header + strlen(g_injected_header), '\n');
    injected_lines_ = header_lines + fwd_lines;
    return result;
}

bool Preprocessor::is_already_transformed(const std::string& source) {
    return source.find("vb_init") != std::string::npos ||
           source.find("ArduinoAPI") != std::string::npos;
}

std::string Preprocessor::replace_api_calls(const std::string& source) {
    std::string s = source;

    // Serial -- must do println before print to avoid partial match
    s = replace_all(s, "Serial.println(", "Serial_println(");
    s = replace_all(s, "Serial.print(",   "Serial_print(");
    s = replace_all(s, "Serial.begin(",    "api->Serial_begin(");
    s = replace_all(s, "Serial.available(","api->Serial_available(");
    s = replace_all(s, "Serial.read(",     "api->Serial_read(");

    // GPIO -- digitalRead before digitalWrite to avoid partial match
    s = replace_all(s, "digitalRead(",     "api->digitalRead(");
    s = replace_all(s, "digitalWrite(",    "api->digitalWrite(");
    s = replace_all(s, "pinMode(",         "api->pinMode(");
    s = replace_all(s, "analogRead(",      "api->analogRead(");
    s = replace_all(s, "analogWrite(",     "api->analogWrite(");

    // Timing
    s = replace_all(s, "delayMicroseconds(","api->delayMicroseconds(");
    s = replace_all(s, "delay(",           "api->delay(");
    s = replace_all(s, "micros()",         "api->micros()");
    s = replace_all(s, "millis()",         "api->millis()");
    // pulseIn is handled by an inline wrapper (with default timeout) in inject_header

    // Watch Variable
    s = replace_all(s, "watch_variable(", "api->watch_variable(");

    return s;
}

std::string Preprocessor::strip_includes(const std::string& source) {
    std::string s = source;

    s = replace_all(s, "#include <Servo.h>",
    "#ifndef VB_SERVO_H\n"
    "#define VB_SERVO_H\n"
    "class Servo {\n"
    "public:\n"
    "    void attach(int pin) { pin_ = pin; }\n"
    "    void write(int angle) { if (pin_ >= 0) { angle_ = angle; api->analogWrite(pin_, angle * 255 / 180); } }\n"
    "    int read() const { return angle_; }\n"
    "    bool attached() const { return pin_ >= 0; }\n"
    "    void detach() { pin_ = -1; }\n"
    "private:\n"
    "    int pin_ = -1;\n"
    "    int angle_ = 0;\n"
    "};\n"
    "#endif\n");

    return s;
}

std::string Preprocessor::wrap_functions(const std::string& source) {
    std::string s = source;

    // Match: void setup() with optional whitespace
    // Replace with export decorator + vb_setup
    std::regex setup_re(R"(void\s+setup\s*\(\s*\))");
    s = std::regex_replace(s, setup_re,
        "extern \"C\" VB_EXPORT\nvoid vb_setup()");

    // Match: void loop() with optional whitespace
    std::regex loop_re(R"(void\s+loop\s*\(\s*\))");
    s = std::regex_replace(s, loop_re,
        "extern \"C\" VB_EXPORT\nvoid vb_loop()");

    return s;
}

std::string Preprocessor::inject_header(const std::string& source) {
    return std::string(g_injected_header) + source;
}

std::string Preprocessor::replace_all(const std::string& source,
                                       const std::string& from,
                                       const std::string& to) {
    if (from.empty()) return source;
    std::string result;
    result.reserve(source.size());
    size_t pos = 0;
    while (true) {
        size_t found = source.find(from, pos);
        if (found == std::string::npos) {
            result.append(source, pos, std::string::npos);
            break;
        }
        result.append(source, pos, found - pos);
        result.append(to);
        pos = found + from.size();
    }
    return result;
}

std::string Preprocessor::inject_safety_delay(const std::string& source) {
    // If loop already has a delay, no need to inject
    bool has_delay = source.find("api->delay(") != std::string::npos;
    if (has_delay) return source;

    // Find the last closing brace -- that's the end of vb_loop()
    size_t last_brace = source.rfind('}');
    if (last_brace == std::string::npos) return source;

    // Insert safety delay just before the last closing brace
    std::string result = source;
    result.insert(last_brace, "    api->delay(10);\n");
    return result;
}

std::string Preprocessor::generate_forward_declarations(const std::string& source) {
    // Match top-level (non-indented) function definitions with common Arduino return types.
    // This mimics what the Arduino IDE does automatically -- allows calling functions
    // before they are defined in the source file.
    std::regex func_re(
        R"(^((?:void|int|float|double|bool|long|char|byte|String)\s+(\w+)\s*\([^)]*\))\s*\{)",
        std::regex::multiline
    );

    std::string decls;
    auto begin = std::sregex_iterator(source.begin(), source.end(), func_re);
    auto end_it = std::sregex_iterator();

    for (auto it = begin; it != end_it; ++it) {
        std::smatch m = *it;
        std::string name = m[2].str();
        // setup and loop are renamed by wrap_functions -- skip them
        if (name == "setup" || name == "loop") continue;
        decls += m[1].str() + ";\n";
    }

    return decls;
}

std::string Preprocessor::extract_board_profile(const std::string& source) {
    std::regex board_re(R"(//\s*@board\s+(.+))");
    std::smatch match;
    if (std::regex_search(source, match, board_re)) {
        std::string profile_name = match[1].str();
        profile_name.erase(profile_name.find_last_not_of(" \t\r\n") + 1); // trim trailing whitespace
        return profile_name;
    }
    return "";
}
