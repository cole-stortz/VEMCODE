#pragma once
#include <string>
#include <vector>

struct CompileError {
    std::string file;
    int         line    = 0;
    int         column  = 0;
    std::string message;
    bool        is_error = true; // false = warning
};

struct CompileResult {
    bool                     success = false;
    std::string              dll_path;       // path to output DLL if success
    std::string              raw_output;     // full stderr from g++
    std::vector<CompileError> errors;        // parsed errors and warnings
    std::vector<std::string> warnings;       // pre-compile warnings (unsupported includes, etc.)
    int                      header_lines = 131; // injected lines before user code (for error line offsetting)
    std::string              board_hint;     // board name from // @board comment, empty if not found
};

// Synchronous g++ wrapper — call from a background thread.
class Compiler {
public:
    Compiler();

    void set_compiler_path(const std::string& path) { compiler_path_ = path; }
    // Passed as -I so sketches can find arduinoapi.h
    void set_include_path(const std::string& path) { include_path_ = path; }
    // Output directory for the built .so/.dll
    void set_output_dir(const std::string& path) { output_dir_ = path; }

    CompileResult compile(const std::string& sketch_path);

private:
    std::string compiler_path_;
    std::string include_path_;
    std::string output_dir_;

    std::vector<CompileError> parse_errors(const std::string& stderr_output);
    std::string run_command(const std::string& cmd);
};