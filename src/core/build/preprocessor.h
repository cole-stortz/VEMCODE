#pragma once
#include <string>
#include <vector>

extern const char* g_injected_header;
extern const char* g_servo_lib;
extern const char* g_liquidcrystal_lib;
extern const char* g_softwareserial_lib;
extern const char* g_wire_lib;
extern const char* g_spi_lib;

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
    // Warnings collected during process() — unsupported #includes, etc.
    std::vector<std::string> takeWarnings() { return std::move(warnings_); }

private:
    int injected_lines_ = 0;
    std::vector<std::string> warnings_;
    std::vector<std::string> isr_vectors_; // vector names found by transform_isr_blocks

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

    // Inject api->delay(1) into while loop bodies that have no delay
    std::string inject_while_delays(const std::string& source);

    // Replace embedded includes with either nothing or custom functions if needed
    std::string strip_includes(const std::string& source);

    // Transform ISR(X_vect) { } blocks into plain functions + populate isr_vectors_
    std::string transform_isr_blocks(const std::string& source);

    // Inject api->register_isr() calls into vb_setup() for each found ISR vector
    std::string inject_isr_registrations(const std::string& source);

    // Strip / translate __asm__ / asm blocks
    std::string transform_asm_blocks(const std::string& source);

    // Scan source for top-level function definitions and return forward declarations.
    // Mimics what the Arduino IDE does automatically.
    std::string generate_forward_declarations(const std::string& source);
};