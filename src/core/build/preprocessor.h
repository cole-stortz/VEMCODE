#pragma once
#include <string>

// Preprocessor transforms standard Arduino sketch syntax into
// VirtualBench DLL format before compilation.
//
// Input (Arduino style):
//   void setup() { pinMode(13, OUTPUT); }
//   void loop()  { digitalWrite(13, HIGH); }
//
// Output (VirtualBench DLL style):
//   #include "src/core/runtime/arduinoapi.h"
//   static ArduinoAPI* api = nullptr;
//   extern "C" __declspec(dllexport) void vb_init(ArduinoAPI* a) { api = a; }
//   extern "C" __declspec(dllexport) void vb_setup() { api->pinMode(13, OUTPUT); }
//   extern "C" __declspec(dllexport) void vb_loop()  { api->digitalWrite(13, HIGH); }
//
// If the source already contains "vb_init" it is returned unchanged --
// so existing VirtualBench format sketches pass through unmodified.
class Preprocessor {
public:
    // Transform Arduino sketch source into VirtualBench DLL source.
    // Returns the transformed source ready to write to a .cpp file.
    std::string process(const std::string& source);
    // Number of lines injected at the top of transformed sketches.
    // Used to offset compiler error line numbers back to the original source.
    static constexpr int INJECTED_HEADER_LINES = 113;

private:
    // Returns true if source is already in VirtualBench format
    bool is_already_transformed(const std::string& source);

    // Replace all Arduino API calls with api-> equivalents
    std::string replace_api_calls(const std::string& source);

    // Wrap setup() and loop() with DLL exports
    std::string wrap_functions(const std::string& source);

    // Inject the VirtualBench header and vb_init boilerplate
    std::string inject_header(const std::string& source);

    // Simple string replace -- replaces all occurrences of from with to
    std::string replace_all(const std::string& source,
                            const std::string& from,
                            const std::string& to);

    // Quick check to see if loop delay exists, add delay(10) if missing
    std::string inject_safety_delay(const std::string& source);
};