#include "src/core/build/preprocessor.h"
#include <regex>
#include <sstream>

std::string Preprocessor::process(const std::string& source) {
    // If already in VirtualBench format, pass through unchanged
    if (is_already_transformed(source))
        return source;

    std::string result = source;
    result = replace_api_calls(result);
    result = wrap_functions(result);
    result = inject_safety_delay(result);  // add this step
    result = inject_header(result);
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

    // Tone
    s = replace_all(s, "tone(",            "api->tone(");
    s = replace_all(s, "noTone(",          "api->noTone(");

    // Watch Variable
    s = replace_all(s, "watch_variable(", "api->watch_variable(");
    return s;
}

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

std::string Preprocessor::inject_header(const std::string& source) {
    std::string header =
        "#include \"src/core/runtime/arduinoapi.h\"\n"
        "#include <string>\n"
        "#include <cstring>\n"
        "using namespace vb;\n\n"
        "static ArduinoAPI* api = nullptr;\n\n"
        "extern \"C\" __declspec(dllexport)\n"
        "void vb_init(ArduinoAPI* a) { api = a; }\n\n"
        "// Serial overloads -- convert any type to string then call through\n"
        "template<typename T>\n"
        "inline void Serial_println(T v) { api->Serial_println(std::to_string(v).c_str()); }\n"
        "inline void Serial_println(const char* s) { api->Serial_println(s); }\n"
        "template<typename T>\n"
        "inline void Serial_print(T v) { api->Serial_print(std::to_string(v).c_str()); }\n"
        "inline void Serial_print(const char* s) { api->Serial_print(s); }\n"
        "\n"
        "// Arduino String class -- wraps std::string for sketch compatibility\n"
        "class String {\n"
        "public:\n"
        "    String() {}\n"
        "    String(const char* s) : s_(s ? s : \"\") {}\n"
        "    String(int v)         : s_(std::to_string(v)) {}\n"
        "    String(long v)        : s_(std::to_string(v)) {}\n"
        "    String(float v)       : s_(std::to_string(v)) {}\n"
        "    String(double v)      : s_(std::to_string(v)) {}\n"
        "    String(std::string s) : s_(s) {}\n"
        "\n"
        "    // Concatenation\n"
        "    String operator+(const String& o) const { return String(s_ + o.s_); }\n"
        "    String operator+(const char* o)   const { return String(s_ + o); }\n"
        "    String& operator+=(const String& o) { s_ += o.s_; return *this; }\n"
        "    String& operator+=(const char* o)   { s_ += o;    return *this; }\n"
        "    String& operator+=(int v)           { s_ += std::to_string(v); return *this; }\n"
        "\n"
        "    // Comparison\n"
        "    bool operator==(const String& o) const { return s_ == o.s_; }\n"
        "    bool operator!=(const String& o) const { return s_ != o.s_; }\n"
        "    bool equals(const String& o)     const { return s_ == o.s_; }\n"
        "\n"
        "    // Info\n"
        "    int    length()    const { return (int)s_.size(); }\n"
        "    bool   isEmpty()   const { return s_.empty(); }\n"
        "    const char* c_str() const { return s_.c_str(); }\n"
        "\n"
        "    // Conversion\n"
        "    int    toInt()     const { return s_.empty() ? 0 : std::stoi(s_); }\n"
        "    float  toFloat()   const { return s_.empty() ? 0.0f : std::stof(s_); }\n"
        "\n"
        "    // Search\n"
        "    int indexOf(const String& s, int from = 0) const {\n"
        "        auto p = s_.find(s.s_, from);\n"
        "        return p == std::string::npos ? -1 : (int)p;\n"
        "    }\n"
        "    int indexOf(char c, int from = 0) const {\n"
        "        auto p = s_.find(c, from);\n"
        "        return p == std::string::npos ? -1 : (int)p;\n"
        "    }\n"
        "    bool startsWith(const String& s) const { return s_.rfind(s.s_, 0) == 0; }\n"
        "    bool endsWith(const String& s)   const {\n"
        "        return s_.size() >= s.s_.size() &&\n"
        "               s_.compare(s_.size() - s.s_.size(), s.s_.size(), s.s_) == 0;\n"
        "    }\n"
        "    bool contains(const String& s)   const { return s_.find(s.s_) != std::string::npos; }\n"
        "\n"
        "    // Manipulation\n"
        "    String substring(int from, int to = -1) const {\n"
        "        if (to < 0) return String(s_.substr(from));\n"
        "        return String(s_.substr(from, to - from));\n"
        "    }\n"
        "    String replace(const String& from, const String& to) const {\n"
        "        std::string r = s_;\n"
        "        size_t pos = 0;\n"
        "        while ((pos = r.find(from.s_, pos)) != std::string::npos) {\n"
        "            r.replace(pos, from.s_.size(), to.s_);\n"
        "            pos += to.s_.size();\n"
        "        }\n"
        "        return String(r);\n"
        "    }\n"
        "    String toLowerCase() const {\n"
        "        std::string r = s_;\n"
        "        for (auto& c : r) c = tolower(c);\n"
        "        return String(r);\n"
        "    }\n"
        "    String toUpperCase() const {\n"
        "        std::string r = s_;\n"
        "        for (auto& c : r) c = toupper(c);\n"
        "        return String(r);\n"
        "    }\n"
        "    String trim() const {\n"
        "        size_t start = s_.find_first_not_of(\" \\t\\n\\r\");\n"
        "        size_t end   = s_.find_last_not_of(\" \\t\\n\\r\");\n"
        "        if (start == std::string::npos) return String(\"\");\n"
        "        return String(s_.substr(start, end - start + 1));\n"
        "    }\n"
        "    char charAt(int i) const { return (i >= 0 && i < (int)s_.size()) ? s_[i] : 0; }\n"
        "    void setCharAt(int i, char c) { if (i >= 0 && i < (int)s_.size()) s_[i] = c; }\n"
        "    char operator[](int i) const  { return charAt(i); }\n"
        "\n"
        "    // Stream support -- allows Serial.println(String)\n"
        "    operator const char*() const { return s_.c_str(); }\n"
        "\n"
        "private:\n"
        "    std::string s_;\n"
        "};\n"
        "\n"
        "// Serial overloads for String type -- must come after String class definition\n"
        "inline void Serial_println(const String& s) { api->Serial_println(s.c_str()); }\n"
        "inline void Serial_print(const String& s)   { api->Serial_print(s.c_str()); }\n"
        "\n";

    return header + source;
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