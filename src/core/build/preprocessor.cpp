#include "src/core/build/preprocessor.h"
#include <cstring>
#include <regex>

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

    // Arduino binary literals: B00101 → 0b00101 (must run before other replacements)
    s = std::regex_replace(s, std::regex("\\bB([01]{1,8})\\b"), "0b$1");

    // Serial -- must do println before print to avoid partial match.
    // replace_token skips matches preceded by a word char so "mySerial.println"
    // is not incorrectly rewritten as "mySerial_println".
    s = replace_token(s, "Serial.println(", "Serial_println(");
    s = replace_token(s, "Serial.print(",   "Serial_print(");
    s = replace_token(s, "Serial.begin(",    "api->Serial_begin(");
    s = replace_token(s, "Serial.available(","api->Serial_available(");
    s = replace_token(s, "Serial.read(",     "api->Serial_read(");

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

    // tone/noTone are handled by inline wrappers (with default duration) in injected_header

    // Interupts
    s = replace_all(s, "attachInterrupt(",   "api->attachInterrupt(");
    s = replace_all(s, "noInterrupts()",   "api->noInterrupts()");
    s = replace_all(s, "interrupts()",     "api->interrupts()");

    // EEPROM
    s = replace_all(s, "EEPROM.write(",    "api->EEPROM_write(");
    s = replace_all(s, "EEPROM.read(",     "api->EEPROM_read(");
    s = replace_all(s, "EEPROM.update(",   "api->EEPROM_update(");

    // Serial1 and Serial2
    s = replace_token(s, "Serial1.begin(",    "api->Serial1_begin(");
    s = replace_token(s, "Serial1.print(",   "api->Serial1_print(");
    s = replace_token(s, "Serial1.println(", "api->Serial1_println(");
    s = replace_token(s, "Serial2.begin(",    "api->Serial2_begin(");
    s = replace_token(s, "Serial2.print(",   "api->Serial2_print(");
    s = replace_token(s, "Serial2.println(", "api->Serial2_println(");

    return s;
}

std::string Preprocessor::strip_includes(const std::string& source) {
    std::string s = source;

    s = replace_all(s, "#include <LiquidCrystal.h>",
    "#ifndef VB_LIQUIDCRYSTAL_H\n"
    "#define VB_LIQUIDCRYSTAL_H\n"
    "#include <string>\n"
    "#include <cstring>\n"
    "class LiquidCrystal {\n"
    "public:\n"
    "    LiquidCrystal(int rs, int, int, int, int, int) : rs_(rs), row_(0), col_(0) {\n"
    "        memset(buf_, ' ', sizeof(buf_)); buf_[0][16]=buf_[1][16]='\\0'; }\n"
    "    void begin(int, int) { if (api) { api->digitalWrite(rs_, 1); flush_(0); flush_(1); } }\n"
    "    void clear() {\n"
    "        memset(buf_, ' ', sizeof(buf_)); buf_[0][16]=buf_[1][16]='\\0';\n"
    "        row_=0; col_=0;\n"
    "        if (api) { flush_(0); flush_(1); } }\n"
    "    void setCursor(int col, int row) { col_=col<16?col:15; row_=row<2?row:1; }\n"
    "    void write(uint8_t c) {\n"
    "        if (col_ >= 16) return;\n"
    "        buf_[row_][col_++] = (c < 32) ? '*' : (char)c;\n"
    "        if (api) flush_(row_); }\n"
    "    void write(char c) { write((uint8_t)c); }\n"
    "    void print(const char* s) { if (!s) return; while (*s) write((uint8_t)*s++); }\n"
    "    void print(const String& s) { print(s.c_str()); }\n"
    "    void print(int v)           { auto s=std::to_string(v); print(s.c_str()); }\n"
    "    void print(long v)          { auto s=std::to_string(v); print(s.c_str()); }\n"
    "    void print(unsigned long v) { auto s=std::to_string(v); print(s.c_str()); }\n"
    "    void print(float v)         { auto s=std::to_string(v); print(s.c_str()); }\n"
    "    void createChar(uint8_t, uint8_t*) {}\n"
    "private:\n"
    "    int rs_, row_, col_;\n"
    "    char buf_[2][17];\n"
    "    void flush_(int row) { if (api) api->lcd_print(rs_, row, buf_[row]); }\n"
    "};\n"
    "#endif\n");

    s = replace_all(s, "#include <EEPROM.h>", "");

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

    s = replace_all(s, "#include <SoftwareSerial.h>",
    "#ifndef VB_SOFTWARESERIAL_H\n"
    "#define VB_SOFTWARESERIAL_H\n"
    "class SoftwareSerial {\n"
    "public:\n"
    "    SoftwareSerial(int rxPin, int txPin) : rxPin_(rxPin), txPin_(txPin) {}\n"
    "    void begin(int baud) { if (api) api->soft_serial_begin(rxPin_, baud); }\n"
    "    void print(const char* s)   { if (api) api->soft_serial_print(rxPin_, s ? s : \"\"); }\n"
    "    void print(const String& s) { if (api) api->soft_serial_print(rxPin_, s.c_str()); }\n"
    "    void print(int v)           { if (api) { auto s = std::to_string(v); api->soft_serial_print(rxPin_, s.c_str()); } }\n"
    "    void print(long v)          { if (api) { auto s = std::to_string(v); api->soft_serial_print(rxPin_, s.c_str()); } }\n"
    "    void print(unsigned long v) { if (api) { auto s = std::to_string(v); api->soft_serial_print(rxPin_, s.c_str()); } }\n"
    "    void print(float v)         { if (api) { auto s = std::to_string(v); api->soft_serial_print(rxPin_, s.c_str()); } }\n"
    "    void println(const char* s)   { if (api) api->soft_serial_println(rxPin_, s ? s : \"\"); }\n"
    "    void println(const String& s) { if (api) api->soft_serial_println(rxPin_, s.c_str()); }\n"
    "    void println(int v)           { if (api) { auto s = std::to_string(v); api->soft_serial_println(rxPin_, s.c_str()); } }\n"
    "    void println(long v)          { if (api) { auto s = std::to_string(v); api->soft_serial_println(rxPin_, s.c_str()); } }\n"
    "    void println(unsigned long v) { if (api) { auto s = std::to_string(v); api->soft_serial_println(rxPin_, s.c_str()); } }\n"
    "    void println(float v)         { if (api) { auto s = std::to_string(v); api->soft_serial_println(rxPin_, s.c_str()); } }\n"
    "    int available() { return api ? api->soft_serial_available(rxPin_) : 0; }\n"
    "    int read() { return api ? api->soft_serial_read(rxPin_) : -1; }\n"
    "    int peek() { return api ? api->soft_serial_peek(rxPin_) : -1; }\n"
    "    void write(uint8_t b) { if (api) { char s[2] = {(char)b, '\\0'}; api->soft_serial_print(rxPin_, s); } }\n"
    "    void write(const uint8_t* buf, size_t n) { for (size_t i = 0; i < n; ++i) write(buf[i]); }\n"
    "    void println() { if (api) api->soft_serial_println(rxPin_, \"\"); }\n"
    "    bool listen() { return true; }\n"
    "    bool isListening() { return true; }\n"
    "    bool overflow() { return false; }\n"
    "private:\n"
    "    int rxPin_, txPin_;\n"
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

std::string Preprocessor::replace_token(const std::string& source,
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
        bool preceded_by_word = (found > 0 &&
            (std::isalnum((unsigned char)source[found - 1]) || source[found - 1] == '_'));
        if (preceded_by_word) {
            result.append(source, pos, found - pos + 1);
            pos = found + 1;
        } else {
            result.append(source, pos, found - pos);
            result.append(to);
            pos = found + from.size();
        }
    }
    return result;
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
