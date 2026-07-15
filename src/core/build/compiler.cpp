#include "src/core/build/compiler.h"
#include "src/core/build/preprocessor.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdio>
#include <filesystem>

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

    // e.g. sketches/blink/blink.cpp → ./blink.so
    std::string sketch_name = sketch_path;
    size_t slash = sketch_name.find_last_of("/\\");
    if (slash != std::string::npos)
        sketch_name = sketch_name.substr(slash + 1);
    size_t dot = sketch_name.find_last_of('.');
    if (dot != std::string::npos)
        sketch_name = sketch_name.substr(0, dot);

    result.dll_path = output_dir_ + "/" + sketch_name + LIB_EXT;

    std::ifstream sketch_file(sketch_path);
    std::string source((std::istreambuf_iterator<char>(sketch_file)),
                        std::istreambuf_iterator<char>());
    sketch_file.close();

    // Pre-flight: require setup() and loop() before wasting time on a full compile
    {
        std::regex setup_re(R"(\bvoid\s+setup\s*\()");
        std::regex loop_re(R"(\bvoid\s+loop\s*\()");
        if (!std::regex_search(source, setup_re)) {
            result.raw_output = "Sketch is missing a setup() function — every Arduino sketch must have void setup() { }";
            return result;
        }
        if (!std::regex_search(source, loop_re)) {
            result.raw_output = "Sketch is missing a loop() function — every Arduino sketch must have void loop() { }";
            return result;
        }
    }

    Preprocessor preprocessor;

    // Extract // @board <name> hint and surface it to the caller via result
    result.board_hint = preprocessor.extract_board_profile(source);

    std::string transformed = preprocessor.process(source);
    result.header_lines = preprocessor.injectedLines();
    result.warnings = preprocessor.takeWarnings();

    std::string temp_path = output_dir_ + "/_vb_temp.cpp";
    std::ofstream temp_file(temp_path);
    temp_file << transformed;
    temp_file.close(); 

    // Collect any extra .cpp files in the sketch folder (multi-file sketch support).
    // The main sketch file is already preprocessed into _vb_temp.cpp, so skip both.
    std::string extra_sources;
    try {
        namespace fs = std::filesystem;
        for (const auto& entry : fs::directory_iterator(output_dir_)) {
            if (entry.path().extension() != ".cpp") continue;
            std::string fname = entry.path().filename().string();
            if (fname == "_vb_temp.cpp") continue;
            if (fname == sketch_name + ".cpp") continue;
            extra_sources += " \"" + entry.path().string() + "\"";
        }
    } catch (...) {}

    std::ostringstream cmd;
    cmd << "\"" << compiler_path_ << "\""
        << " -shared"
        << " -fPIC"
        << " -Wall"
#ifdef _WIN32
        << " -static-libgcc"
        << " -static-libstdc++"
        // MinGW doesn't export DLL symbols by default the way a Linux .so
        // does -- Variable Watch's dlsym-by-name polling needs every sketch
        // global visible, not just the VB_EXPORT-marked vb_init/setup/loop.
        << " -Wl,--export-all-symbols"
#endif
        << " -o \"" << result.dll_path << "\""
        << " \"" << temp_path << "\""
        << extra_sources
        << " -I\"" << include_path_ << "\""
        << " -std=c++17"
#ifndef _WIN32
        << " 2>&1"
#endif
        ;

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
