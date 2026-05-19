#include "src/core/build/compiler.h"
#include "src/core/build/preprocessor.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdio>
#include <windows.h>

Compiler::Compiler()
    : compiler_path_("C:/Qt/Tools/mingw1310_64/bin/g++.exe")
    , include_path_(".")
    , output_dir_(".")
{
}

// -------------------------------------------------------
// compile() -- builds sketch.cpp into sketch.dll
// -------------------------------------------------------
CompileResult Compiler::compile(const std::string& sketch_path) {
    CompileResult result;

    // Derive output DLL path from sketch filename
    // e.g. sketches/blink/blink.cpp -> ./blink.dll
    std::string sketch_name = sketch_path;
    size_t slash = sketch_name.find_last_of("/\\");
    if (slash != std::string::npos)
        sketch_name = sketch_name.substr(slash + 1);
    size_t dot = sketch_name.find_last_of('.');
    if (dot != std::string::npos)
        sketch_name = sketch_name.substr(0, dot);

    result.dll_path = output_dir_ + "/" + sketch_name + ".dll";

    // Read the sketch source
    std::ifstream sketch_file(sketch_path);
    std::string source((std::istreambuf_iterator<char>(sketch_file)),
                        std::istreambuf_iterator<char>());
    sketch_file.close();

    // Preprocess -- transform Arduino syntax to VirtualBench format
    Preprocessor preprocessor;
    std::string transformed = preprocessor.process(source);

    // Write transformed source to a temp file for compilation
    std::string temp_path = output_dir_ + "/_vb_temp.cpp";
    std::ofstream temp_file(temp_path);
    temp_file << transformed;
    temp_file.close();

    // Build the g++ command
    // -shared          = build a DLL not an exe
    // -I include_path_ = find arduinoapi.h from project root
    // -std=c++17       = C++17 features
    // 2>&1             = merge stderr into stdout so we can capture errors
    std::ostringstream cmd;
    cmd << "\"" << compiler_path_ << "\""
        << " -shared"
        << " -static-libgcc"
        << " -static-libstdc++"
        << " -o \"" << result.dll_path << "\""
        << " \"" << temp_path << "\""          // <- temp file not original
        << " -I\"" << include_path_ << "\""
        << " -std=c++17";

    result.raw_output = run_command(cmd.str());
    result.errors     = parse_errors(result.raw_output);

    // If any errors (not just warnings) exist, compilation failed
    result.success = true;
    for (const auto& e : result.errors) {
        if (e.is_error) {
            result.success = false;
            break;
        }
    }

    // Additional check: if output contains "error:" anywhere, it failed
    if (result.raw_output.find(": error:") != std::string::npos)
        result.success = false;

    return result;  
}

// -------------------------------------------------------
// run_command() -- runs a shell command and captures output
// Uses Windows CreateProcess for reliable capture
// -------------------------------------------------------
std::string Compiler::run_command(const std::string& cmd) {
    std::string output;

    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // Create a pipe to capture stdout/stderr
    HANDLE pipe_read, pipe_write;
    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0))
        return "ERROR: Could not create pipe";

    SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb          = sizeof(STARTUPINFOA);
    si.hStdOutput  = pipe_write;
    si.hStdError   = pipe_write;
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // don't flash a console window

    PROCESS_INFORMATION pi = {};

    std::string cmd_copy = cmd;
    BOOL created = CreateProcessA(
        nullptr,
        cmd_copy.data(),
        nullptr, nullptr,
        TRUE,           // inherit handles
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi
    );

    CloseHandle(pipe_write);

    if (!created) {
        CloseHandle(pipe_read);
        return "ERROR: Could not launch compiler";
    }

    // Read all output from the pipe
    char buf[4096];
    DWORD bytes_read;
    while (ReadFile(pipe_read, buf, sizeof(buf) - 1, &bytes_read, nullptr) && bytes_read > 0) {
        buf[bytes_read] = '\0';
        output += buf;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(pipe_read);

    return output;
}

// -------------------------------------------------------
// parse_errors() -- turns g++ stderr into CompileError structs
//
// g++ error format:
//   filename.cpp:line:col: error: message
//   filename.cpp:line:col: warning: message
// -------------------------------------------------------
std::vector<CompileError> Compiler::parse_errors(const std::string& output) {
    std::vector<CompileError> errors;

    // Match:  file:line:col: (error|warning): message
    std::regex pattern(R"(([^:]+):(\d+):(\d+):\s*(error|warning):\s*(.+))");
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, pattern)) {
            CompileError err;
            err.file     = match[1].str();
            err.line     = std::stoi(match[2].str());
            err.column   = std::stoi(match[3].str());
            err.is_error = (match[4].str() == "error");
            err.message  = match[5].str();
            errors.push_back(err);
        }
    }

    return errors;
}