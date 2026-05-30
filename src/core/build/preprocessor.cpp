#include "src/core/build/preprocessor.h"
#include <regex>
#include <sstream>

std::string Preprocessor::process(const std::string& source) {
    // If already in VirtualBench format, pass through unchanged
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

    injected_lines_ = INJECTED_HEADER_LINES + fwd_lines;
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
    std::string header =
        "#include \"src/core/runtime/arduinoapi.h\"\n"
        "#include <string>\n"
        "#include <cstring>\n"
        "using namespace vb;\n\n"
        "static ArduinoAPI* api = nullptr;\n\n"
        "extern \"C\" VB_EXPORT\n"
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
        "\n"
        "// Arduino math functions\n"
        "inline long map(long x, long in_min, long in_max, long out_min, long out_max) {\n"
        "    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;\n"
        "}\n"
        "inline long constrain(long x, long lo, long hi) {\n"
        "    return x < lo ? lo : x > hi ? hi : x;\n"
        "}\n"
        "inline long   vb_abs(long x)         { return x < 0 ? -x : x; }\n"
        "inline long   vb_min(long a, long b) { return a < b ? a : b; }\n"
        "inline long   vb_max(long a, long b) { return a > b ? a : b; }\n"
        "inline long   random(long max)       { return rand() % max; }\n"
        "inline long   random(long min, long max) { return min + rand() % (max - min); }\n"
        "#define abs(x) vb_abs(x)\n"
        "#define min(a,b) vb_min(a,b)\n"
        "#define max(a,b) vb_max(a,b)\n"
        "\n"
        "// pulseIn with optional timeout matching real Arduino behavior\n"
        "inline unsigned long pulseIn(int pin, int value, unsigned long timeout = 1000000UL) {\n"
        "    return api->pulseIn(pin, value, timeout);\n"
        "}\n"
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