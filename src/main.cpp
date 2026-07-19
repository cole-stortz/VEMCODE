#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QIcon>
#include <QStyleFactory>
#include "src/ui/mainwindow.h"
#include "src/core/build/compiler.h"
#include "src/core/circuit/circuitdetector.h"
#include "src/core/host/sketchhost.h"
#include "src/core/runtime/boardprofile.h"
#include <csignal>
#include <atomic>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>

static std::atomic<bool>   g_stop_requested{false};
static ArduinoRuntime*     g_headless_runtime = nullptr;

static void handle_sigint(int) {
    g_stop_requested = true;
    if (g_headless_runtime) g_headless_runtime->request_stop();
}

// If `given` doesn't exist as-is, search the configured sketch location
// recursively for a file with that basename so `vemcode InputTest.cpp` works
// without a full path.
static bool resolve_sketch_path(const std::string& given, std::string& resolved) {
    if (std::filesystem::exists(given)) {
        resolved = given;
        return true;
    }

    QSettings settings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);
    QString default_location = settings.value("sketches/default_location",
        QCoreApplication::applicationDirPath() + "/sketches").toString();
    std::string search_root = default_location.toStdString();
    std::string wanted_name = std::filesystem::path(given).filename().string();
    std::vector<std::string> matches;

    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(search_root, ec);
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        if (it->is_regular_file() && it->path().filename() == wanted_name)
            matches.push_back(it->path().string());
    }

    if (matches.size() == 1) {
        resolved = matches[0];
        return true;
    }
    if (matches.empty()) {
        std::cerr << "Can't find '" << given << "' as a path or under " << search_root << "\n";
    } else {
        std::cerr << "'" << wanted_name << "' is ambiguous -- multiple matches under " << search_root << ":\n";
        for (const auto& m : matches) std::cerr << "  " << m << "\n";
    }
    return false;
}

// Compiles and runs a single sketch on the current thread with no GUI,
// streaming Serial output to stdout until Ctrl+C. Mirrors the compile/detect/
// run sequence MainWindow::onRunClicked drives interactively.
static int run_headless(const std::string& given_path) {
    std::string sketch_path;
    if (!resolve_sketch_path(given_path, sketch_path))
        return 1;

    QSettings settings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);
    std::string compilerPath = settings.value("compiler/path", "").toString().toStdString();
    std::string projectRoot  = settings.value("compiler/project_root", "").toString().toStdString();
    if (compilerPath.empty() || projectRoot.empty()) {
        QString detectedPath, detectedRoot;
        if (!SettingsDialog::autoDetectCompiler(detectedPath, detectedRoot)) {
            std::cerr << "No compiler configured, and couldn't auto-detect one -- "
                          "run VEMCODE with no arguments once to set it up manually.\n";
            return 1;
        }
        compilerPath = detectedPath.toStdString();
        projectRoot  = detectedRoot.toStdString();
        std::cout << "Auto-detected compiler: " << compilerPath
                   << " (project root: " << projectRoot << ")\n";
        settings.setValue("compiler/path", detectedPath);
        settings.setValue("compiler/project_root", detectedRoot);
    }

    std::string boardName = settings.value("board/name", "Arduino Uno").toString().toStdString();
    BoardProfile profile = BOARD_UNO;
    if      (boardName == "Arduino Nano")       profile = BOARD_NANO;
    else if (boardName == "Arduino Mega 2560")  profile = BOARD_MEGA;
    else if (boardName == "Arduino Due")        profile = BOARD_DUE;
    else if (boardName == "Teensy 4.1")         profile = BOARD_TEENSY;

    std::ifstream sketch_file(sketch_path);
    if (!sketch_file) {
        std::cerr << "Can't open sketch: " << sketch_path << "\n";
        return 1;
    }
    std::stringstream source_buf;
    source_buf << sketch_file.rdbuf();
    std::string source = source_buf.str();

    std::string sketch_dir = sketch_path;
    size_t slash = sketch_dir.find_last_of("/\\");
    sketch_dir = (slash != std::string::npos) ? sketch_dir.substr(0, slash) : ".";

    Compiler compiler;
    compiler.set_compiler_path(compilerPath);
    compiler.set_include_path(projectRoot);
    compiler.set_output_dir(sketch_dir);
    CompileResult result = compiler.compile(sketch_path);

    for (const auto& w : result.warnings)
        std::cout << w << "\n";

    if (!result.board_hint.empty()) {
        const std::string& hint = result.board_hint;
        if      (hint == "Arduino Nano")       profile = BOARD_NANO;
        else if (hint == "Arduino Mega 2560")  profile = BOARD_MEGA;
        else if (hint == "Arduino Due")        profile = BOARD_DUE;
        else if (hint == "Teensy 4.1")         profile = BOARD_TEENSY;
        else if (hint == "Arduino Uno")        profile = BOARD_UNO;
    }

    if (!result.success) {
        std::cerr << "=== Compile errors ===\n" << result.raw_output << "\n";
        return 1;
    }

    CircuitDetector detector;
    detector.detect(source);
    std::cout << "=== Components detected ===\n";
    for (const auto& comp : detector.components())
        std::cout << comp.to_string() << "\n";
    std::cout << "===========================\n";
    for (const auto& w : detector.warnings())
        std::cout << w << "\n";

    SketchHost host;
    host.setProfile(profile);

    ArduinoRuntime& runtime = host.runtime();
    runtime.on_serial_output  = [](const std::string& t) { std::cout << t; std::cout.flush(); };
    runtime.on_serial1_output = [](const std::string& t) { std::cout << t; std::cout.flush(); };
    runtime.on_serial2_output = [](const std::string& t) { std::cout << t; std::cout.flush(); };
    // These three have no std::cout fallback in the runtime, unlike Serial/watch_variable.
    runtime.on_lcd_print = [](int pin, int row, const std::string& text) {
        std::cout << "  LCD[" << pin << "] row" << row << ": \"" << text << "\"\n";
    };
    runtime.on_watchdog_reset = []() {
        std::cout << "  WATCHDOG RESET\n";
    };
    runtime.on_sleep_changed = [](bool sleeping) {
        std::cout << "  sleep: " << (sleeping ? "entering" : "woke") << "\n";
    };

    if (!host.load(result.dll_path)) {
        std::cerr << "Failed to load compiled sketch\n";
        return 1;
    }

    g_headless_runtime = &runtime;
    std::signal(SIGINT, handle_sigint);

    while (!g_stop_requested) {
        std::lock_guard<std::mutex> exec_lock(runtime.exec_mutex());
        host.run_loop();
    }

    std::cout << "\nStopped.\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("VEMCODE");
        app.setApplicationVersion("0.1");
        return run_headless(argv[1]);
    }

    QApplication app(argc, argv);
    // The Fusion style fully honors QPalette/QSS everywhere; native platform
    // styles (GTK/Breeze/Adwaita integration) partially ignore both for some
    // native-rendered elements (checkbox labels, header sections, tab bar
    // fill), which is what caused patches of the light theme to stay dark.
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setApplicationName("VEMCODE");
    app.setApplicationVersion("0.1");
    app.setWindowIcon(QIcon(":/logo.svg"));

    MainWindow window;
    window.show();

    return app.exec();
}
