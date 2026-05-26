#pragma once
#include <string>
#include <vector>

// Represents a single compiler error or warning
struct CompileError {
    std::string file;
    int         line    = 0;
    int         column  = 0;
    std::string message;
    bool        is_error = true; // false = warning
};

// Result returned by Compiler::compile()
struct CompileResult {
    bool                     success = false;
    std::string              dll_path;       // path to output DLL if success
    std::string              raw_output;     // full stderr from g++
    std::vector<CompileError> errors;        // parsed errors and warnings
    int                      header_lines = 131; // injected lines before user code (for error line offsetting)
};

// Compiler wraps g++ to build a sketch .cpp file into a .dll.
// It is synchronous -- call it from a background thread (CompilerThread).
//
// Usage:
//   Compiler compiler;
//   compiler.set_compiler_path("C:/Qt/Tools/mingw1310_64/bin/g++.exe");
//   compiler.set_include_path("C:/Users/.../VirtualBench");
//   CompileResult result = compiler.compile("sketches/blink/blink.cpp");
class Compiler {
public:
    Compiler();

    // Path to g++ executable
    void set_compiler_path(const std::string& path) { compiler_path_ = path; }

    // Project root -- passed as -I to g++ so sketches can find arduinoapi.h
    void set_include_path(const std::string& path) { include_path_ = path; }

    // Output directory for the built DLL (defaults to project root)
    void set_output_dir(const std::string& path) { output_dir_ = path; }

    // Compile a sketch .cpp file into a .dll
    // Blocks until compilation finishes
    CompileResult compile(const std::string& sketch_path);

private:
    std::string compiler_path_;
    std::string include_path_;
    std::string output_dir_;

    // Parse g++ stderr into structured CompileError objects
    std::vector<CompileError> parse_errors(const std::string& stderr_output);

    // Run a command and capture its stdout+stderr
    std::string run_command(const std::string& cmd);
};