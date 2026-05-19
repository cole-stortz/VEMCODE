#include "src/core/build/preprocessor.h"
#include <regex>
#include <sstream>

// -------------------------------------------------------
// process() -- main entry point
// -------------------------------------------------------
std::string Preprocessor::process(const std::string& source) {
    // If already in VirtualBench format, pass through unchanged
    if (is_already_transformed(source))
        return source;

    std::string result = source;
    result = replace_api_calls(result);
    result = wrap_functions(result);
    result = inject_header(result);
    return result;
}

// -------------------------------------------------------
// is_already_transformed() -- detect VirtualBench format
// -------------------------------------------------------
bool Preprocessor::is_already_transformed(const std::string& source) {
    return source.find("vb_init") != std::string::npos ||
           source.find("ArduinoAPI") != std::string::npos;
}

// -------------------------------------------------------
// replace_api_calls() -- replaces Arduino API with api->
//
// Order matters -- longer/more specific patterns first
// to avoid partial replacements (e.g. Serial.println before Serial.print)
// -------------------------------------------------------
std::string Preprocessor::replace_api_calls(const std::string& source) {
    std::string s = source;

    // Serial -- must do println before print to avoid partial match
    s = replace_all(s, "Serial.println(",  "api->Serial_println(");
    s = replace_all(s, "Serial.print(",    "api->Serial_print(");
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

    // Tone
    s = replace_all(s, "tone(",            "api->tone(");
    s = replace_all(s, "noTone(",          "api->noTone(");

    return s;
}

// -------------------------------------------------------
// wrap_functions() -- renames setup() and loop() to
// vb_setup() and vb_loop() with DLL export decorators
// -------------------------------------------------------
std::string Preprocessor::wrap_functions(const std::string& source) {
    std::string s = source;

    // Match: void setup() with optional whitespace
    // Replace with export decorator + vb_setup
    std::regex setup_re(R"(void\s+setup\s*\(\s*\))");
    s = std::regex_replace(s, setup_re,
        "extern \"C\" __declspec(dllexport)\nvoid vb_setup()");

    // Match: void loop() with optional whitespace
    std::regex loop_re(R"(void\s+loop\s*\(\s*\))");
    s = std::regex_replace(s, loop_re,
        "extern \"C\" __declspec(dllexport)\nvoid vb_loop()");

    return s;
}

// -------------------------------------------------------
// inject_header() -- prepends boilerplate at the top
// -------------------------------------------------------
std::string Preprocessor::inject_header(const std::string& source) {
    std::string header =
        "#include \"src/core/runtime/arduinoapi.h\"\n"
        "#include <string>\n"
        "#include <cstring>\n"
        "using namespace vb;\n\n"
        "static ArduinoAPI* api = nullptr;\n\n"
        "extern \"C\" __declspec(dllexport)\n"
        "void vb_init(ArduinoAPI* a) { api = a; }\n\n";

    return header + source;
}

// -------------------------------------------------------
// replace_all() -- replaces every occurrence of from with to
// -------------------------------------------------------
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