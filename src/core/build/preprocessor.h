#pragma once
#include <string>

extern const char* g_injected_header;

// Transforms standard Arduino sketch syntax into DLL format before compilation.
// sketch:  void setup() { pinMode(13, OUTPUT); }
// becomes: extern "C" void vb_setup() { api->pinMode(13, OUTPUT); }
// Passes through unchanged if source already contains "vb_init".
class Preprocessor {
public:
    std::string process(const std::string& source);
    // Lines injected by the last process() call — used to offset compiler error line numbers.
    int injectedLines() const { return injected_lines_; }
    std::string extract_board_profile(const std::string& source);

private:
    int injected_lines_ = 0;

    // Returns true if source is already in VirtualEmbeddedProgrammer format
    bool is_already_transformed(const std::string& source);

    // Replace all Arduino API calls with api-> equivalents
    std::string replace_api_calls(const std::string& source);

    // Wrap setup() and loop() with DLL exports
    std::string wrap_functions(const std::string& source);

    // Inject the VirtualEmbeddedProgrammer header and vb_init boilerplate
    std::string inject_header(const std::string& source);

    // Simple string replace -- replaces all occurrences of from with to
    std::string replace_all(const std::string& source,
                            const std::string& from,
                            const std::string& to);

    // Like replace_all but skips matches where the character immediately before
    // the match is a word character (letter, digit, _). Prevents "Serial." from
    // matching inside "mySerial." or other identifiers ending in "Serial".
    std::string replace_token(const std::string& source,
                              const std::string& from,
                              const std::string& to);

    // Quick check to see if loop delay exists, add delay(10) if missing
    std::string inject_safety_delay(const std::string& source);

    // Replace embedded includes with either nothing or custom functions if needed
    std::string strip_includes(const std::string& source);

    // Scan source for top-level function definitions and return forward declarations.
    // Mimics what the Arduino IDE does automatically.
    std::string generate_forward_declarations(const std::string& source);
};