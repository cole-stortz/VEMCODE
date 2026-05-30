#include "src/core/build/compiler.h"
#include "src/core/build/preprocessor.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

// Platform shared-library extension
#ifdef _WIN32
static const std::string LIB_EXT = ".dll";
#elif defined(__APPLE__)
static const std::string LIB_EXT = ".dylib";
#else
static const std::string LIB_EXT = ".so";
#endif

Compiler::Compiler()
    : compiler_path_("g++")
    , include_path_(".")
    , output_dir_(".")
{
}

CompileResult Compiler::compile(const std::string& sketch_path) {
    CompileResult result;

    // Derive output library path from sketch filename
    // e.g. sketches/blink/blink.cpp -> ./blink.so (or .dll on Windows)
    std::string sketch_name = sketch_path;
    size_t slash = sketch_name.find_last_of("/\\");
    if (slash != std::string::npos)
        sketch_name = sketch_name.substr(slash + 1);
    size_t dot = sketch_name.find_last_of('.');
    if (dot != std::string::npos)
        sketch_name = sketch_name.substr(0, dot);

    result.dll_path = output_dir_ + "/" + sketch_name + LIB_EXT;

    // Read the sketch source
    std::ifstream sketch_file(sketch_path);
    std::string source((std::istreambuf_iterator<char>(sketch_file)),
                        std::istreambuf_iterator<char>());
    sketch_file.close();

    // Preprocess -- transform Arduino syntax to VirtualBench format
    Preprocessor preprocessor;
    std::string transformed = preprocessor.process(source);
    result.header_lines = preprocessor.injectedLines();

    // Write transformed source to a temp file for compilation
    std::string temp_path = output_dir_ + "/_vb_temp.cpp";
    std::ofstream temp_file(temp_path);
    temp_file << transformed;
    temp_file.close();

    // Build the g++ command
    // -shared  = build a shared library (.so/.dll)
    // -fPIC    = position-independent code (required on Linux/Mac)
    // -I       = find arduinoapi.h from project root
    std::ostringstream cmd;
    cmd << "\"" << compiler_path_ << "\""
        << " -shared"
        << " -fPIC"
#ifdef _WIN32
        << " -static-libgcc"
        << " -static-libstdc++"
#endif
        << " -o \"" << result.dll_path << "\""
        << " \"" << temp_path << "\""
        << " -I\"" << include_path_ << "\""
        << " -std=c++17"
        << " 2>&1";

    result.raw_output = run_command(cmd.str());
    result.errors     = parse_errors(result.raw_output);

    result.success = true;
    for (const auto& e : result.errors) {
        if (e.is_error) {
            result.success = false;
            break;
        }
    }

    if (result.raw_output.find(": error:") != std::string::npos)
        result.success = false;

    return result;
}

std::string Compiler::run_command(const std::string& cmd) {
#ifdef _WIN32
    std::string output;

    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE pipe_read, pipe_write;
    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0))
        return "ERROR: Could not create pipe";

    SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb          = sizeof(STARTUPINFOA);
    si.hStdOutput  = pipe_write;
    si.hStdError   = pipe_write;
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmd_copy = cmd;
    BOOL created = CreateProcessA(
        nullptr, cmd_copy.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );

    CloseHandle(pipe_write);

    if (!created) {
        CloseHandle(pipe_read);
        return "ERROR: Could not launch compiler";
    }

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
#else
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return "ERROR: Could not launch compiler";
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);
    return output;
#endif
}


std::vector<CompileError> Compiler::parse_errors(const std::string& output) {
    std::vector<CompileError> errors;

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
