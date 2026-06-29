#include "src/core/build/preprocessor.h"
#include <cstring>
#include <regex>

std::string Preprocessor::process(const std::string& source) {
    // If already in VirtualEmbeddedProgrammer format, pass through unchanged
    if (is_already_transformed(source))
        return source;

    std::string result = source;
    // ISR and asm transforms run first so their output is picked up by replace_api_calls
    result = transform_isr_blocks(result);
    result = transform_asm_blocks(result);
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
    result = inject_isr_registrations(result);
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
    s = replace_token(s, "Serial.printf(",  "Serial_printf(");
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

    // Watchdog
    s = replace_token(s, "wdt_enable(", "api->wdt_enable(");
    s = replace_token(s, "wdt_disable(", "api->wdt_disable(");
    s = replace_token(s, "wdt_reset(", "api->wdt_reset(");
    s = replace_token(s, "set_sleep_mode(", "api->set_sleep_mode(");
    s = replace_token(s, "sleep_enable(", "api->sleep_enable(");
    s = replace_token(s, "sleep_disable(", "api->sleep_disable(");
    s = replace_token(s, "sleep_cpu(", "api->sleep_cpu(");
    

    return s;
}

std::string Preprocessor::strip_includes(const std::string& source) {
    struct LibEntry {
        const char* header;
        const char* content; // nullptr = just strip silently
    };
    static const LibEntry kLibs[] = {
        { "Servo",          g_servo_lib         },
        { "LiquidCrystal",  g_liquidcrystal_lib },
        { "SoftwareSerial", g_softwareserial_lib },
        { "EEPROM",          nullptr },
        { "Arduino",         nullptr },
        { "avr/pgmspace",    nullptr },
        { "avr/interrupt",   nullptr },
        { "avr/io",          nullptr },
        { "util/delay",      nullptr },
        { "avr/wdt",         nullptr },
        { "avr/sleep",       nullptr },
    };

    // Standard C/C++ headers that the host compiler can find — don't warn about these
    static const char* kStdHeaders[] = {
        "stdio.h", "stdlib.h", "string.h", "math.h", "time.h", "stdbool.h",
        "stdint.h", "stddef.h", "assert.h", "limits.h", "float.h", "ctype.h",
        "errno.h", "inttypes.h", "stdarg.h", "stdint.h", "memory.h",
    };

    std::string s = source;

    for (const auto& lib : kLibs) {
        std::string include_str = std::string("#include <") + lib.header + ".h>";
        s = replace_all(s, include_str, lib.content ? lib.content : "");
    }

    // Warn about any remaining #include <X.h> that VEMCODE doesn't know about
    std::regex unknown_include_re(R"(#include\s*<([^>]+)>)");
    auto begin = std::sregex_iterator(s.begin(), s.end(), unknown_include_re);
    auto end_it = std::sregex_iterator();
    for (auto it = begin; it != end_it; ++it) {
        std::string header = (*it)[1].str();
        // Only warn about .h includes (Arduino-style libraries); C++ std headers have no .h
        if (header.size() < 2 || header.substr(header.size() - 2) != ".h")
            continue;
        // Skip known-good C standard headers
        bool is_std = false;
        for (const char* std_h : kStdHeaders) {
            if (header == std_h) { is_std = true; break; }
        }
        if (is_std) continue;
        warnings_.push_back(
            "WARNING: <" + header + "> is not supported by VEMCODE — "
            "calls to this library will not work");
    }

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
    std::string result = source;

    bool has_delay = result.find("api->delay(") != std::string::npos;
    if (!has_delay) {
        // No delay anywhere — inject one at the end of vb_loop()
        size_t last_brace = result.rfind('}');
        if (last_brace != std::string::npos)
            result.insert(last_brace, "    api->delay(10);\n");
    }

    // Also scan every while loop body and inject a delay if missing
    result = inject_while_delays(result);
    return result;
}

std::string Preprocessor::inject_while_delays(const std::string& source) {
    std::string s = source;
    size_t pos = 0;

    while (pos < s.size()) {
        size_t w = s.find("while", pos);
        if (w == std::string::npos) break;

        // Must be a whole-word match
        if (w > 0 && (std::isalnum((unsigned char)s[w - 1]) || s[w - 1] == '_')) {
            pos = w + 5;
            continue;
        }

        // Skip if this is the 'while' at the end of a do...while
        size_t prev = w;
        while (prev > 0 && std::isspace((unsigned char)s[prev - 1])) --prev;
        if (prev > 0 && s[prev - 1] == '}') {
            pos = w + 5;
            continue;
        }

        // Skip whitespace after 'while', then expect '('
        size_t cp = w + 5;
        while (cp < s.size() && std::isspace((unsigned char)s[cp])) ++cp;
        if (cp >= s.size() || s[cp] != '(') { pos = cp; continue; }

        // Find the matching ')' closing the condition
        int depth = 1;
        ++cp;
        while (cp < s.size() && depth > 0) {
            if (s[cp] == '(') ++depth;
            else if (s[cp] == ')') --depth;
            ++cp;
        }

        // Skip whitespace, then expect '{' opening the body
        while (cp < s.size() && std::isspace((unsigned char)s[cp])) ++cp;
        if (cp >= s.size() || s[cp] != '{') { pos = cp; continue; }

        // Find the matching '}' closing the body
        size_t body_open = cp;
        int bdepth = 1;
        size_t bp = body_open + 1;
        while (bp < s.size() && bdepth > 0) {
            if (s[bp] == '{') ++bdepth;
            else if (s[bp] == '}') --bdepth;
            ++bp;
        }
        size_t body_close = bp - 1; // index of '}'

        // Check whether this body already has any delay call
        std::string body = s.substr(body_open + 1, body_close - body_open - 1);
        bool body_has_delay = body.find("api->delay(") != std::string::npos ||
                              body.find("api->delayMicroseconds(") != std::string::npos;

        if (!body_has_delay) {
            s.insert(body_close, "    api->delay(1);\n");
            pos = body_close + 19; // step past the injected text
        } else {
            pos = body_close + 1;
        }
    }

    return s;
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

std::string Preprocessor::transform_isr_blocks(const std::string& source) {
    static const char* kKnownVectors[] = {
        "INT0_vect", "INT1_vect",
        "PCINT0_vect", "PCINT1_vect", "PCINT2_vect",
        "TIMER1_OVF_vect", "TIMER2_OVF_vect",
        "TIMER1_COMPA_vect", "TIMER1_COMPB_vect",
        "USART_RX_vect", "WDT_vect",
        nullptr
    };

    isr_vectors_.clear();
    std::string s = source;
    size_t pos = 0;

    while (pos < s.size()) {
        // Skip line comments
        if (pos + 1 < s.size() && s[pos] == '/' && s[pos+1] == '/') {
            size_t nl = s.find('\n', pos + 2);
            pos = (nl == std::string::npos) ? s.size() : nl + 1;
            continue;
        }
        // Skip block comments
        if (pos + 1 < s.size() && s[pos] == '/' && s[pos+1] == '*') {
            size_t end = s.find("*/", pos + 2);
            pos = (end == std::string::npos) ? s.size() : end + 2;
            continue;
        }
        // Skip string literals
        if (s[pos] == '"') {
            ++pos;
            while (pos < s.size() && s[pos] != '"') {
                if (s[pos] == '\\') ++pos;
                if (pos < s.size()) ++pos;
            }
            if (pos < s.size()) ++pos;
            continue;
        }
        // Check for ISR( at current position
        if (s.size() - pos < 4 || s[pos] != 'I' || s[pos+1] != 'S' || s[pos+2] != 'R' || s[pos+3] != '(') {
            ++pos;
            continue;
        }
        size_t isr_pos = pos;

        // Must be a whole-word match (not e.g. inside TISR or another identifier)
        if (isr_pos > 0 && (std::isalnum((unsigned char)s[isr_pos - 1]) || s[isr_pos - 1] == '_')) {
            pos = isr_pos + 4;
            continue;
        }

        // Extract the vector name from ISR(...)
        size_t name_start = isr_pos + 4;
        size_t close_paren = s.find(')', name_start);
        if (close_paren == std::string::npos) break;

        std::string vect_name = s.substr(name_start, close_paren - name_start);
        // Trim whitespace
        size_t vs = vect_name.find_first_not_of(" \t\n\r");
        size_t ve = vect_name.find_last_not_of(" \t\n\r");
        if (vs == std::string::npos) { pos = close_paren + 1; continue; }
        vect_name = vect_name.substr(vs, ve - vs + 1);

        // Require _vect suffix — rejects false matches like ISR() from user code
        if (vect_name.size() < 5 || vect_name.substr(vect_name.size() - 5) != "_vect") {
            pos = close_paren + 1;
            continue;
        }

        // Warn about vectors we can't simulate
        bool known = false;
        for (int i = 0; kKnownVectors[i]; ++i)
            if (vect_name == kKnownVectors[i]) { known = true; break; }
        if (!known)
            warnings_.push_back(
                "WARNING: ISR vector '" + vect_name + "' is not simulated — the handler will never fire");

        // Find the opening { of the body
        size_t brace_pos = close_paren + 1;
        while (brace_pos < s.size() && std::isspace((unsigned char)s[brace_pos])) ++brace_pos;
        if (brace_pos >= s.size() || s[brace_pos] != '{') { pos = brace_pos; continue; }

        // Find matching }
        int depth = 1;
        size_t bp = brace_pos + 1;
        while (bp < s.size() && depth > 0) {
            if (s[bp] == '{') ++depth;
            else if (s[bp] == '}') --depth;
            ++bp;
        }
        size_t body_end = bp; // one past closing }

        // Build replacement: void __vb_isr_X_vect() { body }
        std::string func_name = "__vb_isr_" + vect_name;
        std::string body = s.substr(brace_pos, body_end - brace_pos);
        std::string replacement = "void " + func_name + "() " + body;

        s.replace(isr_pos, body_end - isr_pos, replacement);
        isr_vectors_.push_back(vect_name);

        pos = isr_pos + replacement.size();
    }

    return s;
}

std::string Preprocessor::inject_isr_registrations(const std::string& source) {
    if (isr_vectors_.empty()) return source;

    // Build forward declarations + registration calls
    std::string fwd_decls;
    std::string reg_calls;
    for (const auto& v : isr_vectors_) {
        fwd_decls += "void __vb_isr_" + v + "();\n";
        reg_calls += "    api->register_isr(\"" + v + "\", __vb_isr_" + v + ");\n";
    }

    // Insert registrations at the start of vb_setup() body
    std::string target = "void vb_setup() {";
    size_t setup_pos = source.find(target);
    if (setup_pos == std::string::npos) return source;

    std::string result = fwd_decls + source;
    // Recalculate position after prepending fwd_decls
    setup_pos = result.find(target);
    result.insert(setup_pos + target.size(), "\n" + reg_calls);
    return result;
}

std::string Preprocessor::transform_asm_blocks(const std::string& source) {
    // Single-instruction mapping: asm string → replacement (nullptr = strip silently)
    static const struct { const char* instr; const char* replacement; } kAsmMap[] = {
        { "nop",    nullptr               },
        { "cli",    "noInterrupts()" },
        { "sei",    "interrupts()"  },
        { "sleep",  nullptr               },
        { "wdr",    nullptr               },
        { "rjmp 0", nullptr               },
        { nullptr,  nullptr               }
    };

    // Match: (asm|__asm__) (volatile|__volatile__)? ( "single_instruction" anything );
    // Captures the instruction string in group 1
    std::regex asm_re(
        R"((?:__asm__|asm)\s*(?:__volatile__|volatile)?\s*\(\s*"([^"\\]*)(?:\\[ntr])?"[^;\n]*\)\s*;)"
    );

    std::string result;
    result.reserve(source.size());
    size_t last_pos = 0;

    auto begin = std::sregex_iterator(source.begin(), source.end(), asm_re);
    auto end_it = std::sregex_iterator();

    for (auto it = begin; it != end_it; ++it) {
        const std::smatch& m = *it;
        result.append(source, last_pos, m.position() - last_pos);

        std::string instr = m[1].str();
        // Trim trailing \n \t etc from the instruction literal
        size_t trim = instr.find_last_not_of(" \t\n\r");
        if (trim != std::string::npos) instr = instr.substr(0, trim + 1);

        bool found = false;
        for (int i = 0; kAsmMap[i].instr; ++i) {
            if (instr == kAsmMap[i].instr) {
                found = true;
                if (kAsmMap[i].replacement)
                    result.append(kAsmMap[i].replacement).append(";");
                else if (instr != "nop")
                    warnings_.push_back(
                        "NOTE: Assembly instruction '" + instr + "' removed — no simulation equivalent yet");
                break;
            }
        }
        if (!found)
            warnings_.push_back(
                "WARNING: Unrecognized assembly instruction '" + instr + "' removed — it has no simulation equivalent");

        last_pos = m.position() + m.length();
    }
    result.append(source, last_pos, std::string::npos);

    return result;
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
