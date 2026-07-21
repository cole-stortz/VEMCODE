#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QIcon>
#include <QStyleFactory>
#include "src/ui/mainwindow.h"
#include "src/core/build/compiler.h"
#include "src/core/circuit/circuitdetector.h"
#include "src/core/host/sketchhost.h"
#include "src/core/host/timeline.h"
#include "src/core/runtime/boardprofile.h"
#include "src/appsettings.h"
#include <csignal>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>
#include <memory>
#include <thread>
#include <condition_variable>

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

    QSettings settings = appSettings();
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

struct HeadlessOptions {
    std::string timeline_path;
    bool        auto_timeline   = false; // timeline=true -- use <sketch>.timeline
    double      timeout_seconds = 0;     // 0 = unlimited (Ctrl+C like today)
    float       speed           = 1.0f;  // sketch-time multiplier
};

static bool parse_bool_option(const std::string& value, bool& out) {
    if (value == "true" || value == "1")  { out = true;  return true; }
    if (value == "false" || value == "0") { out = false; return true; }
    return false;
}

// Sibling `<sketch>.timeline` next to the resolved sketch path -- same
// basename, extension swapped, e.g. "Foo/Bar.cpp" -> "Foo/Bar.timeline".
static std::string derive_timeline_path(const std::string& sketch_path) {
    size_t dot = sketch_path.find_last_of('.');
    std::string base = (dot != std::string::npos) ? sketch_path.substr(0, dot) : sketch_path;
    return base + ".timeline";
}

// Parses argv[2..argc-1]: a bare arg (no '=') is the optional timeline file
// path (error if more than one given); a key=value arg sets an option.
// Prints an error and returns false on any bad input.
static bool parse_headless_args(int argc, char* argv[], HeadlessOptions& opts) {
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        size_t eq = arg.find('=');
        if (eq == std::string::npos) {
            if (!opts.timeline_path.empty()) {
                std::cerr << "Only one timeline file may be given (already have '"
                           << opts.timeline_path << "', got '" << arg << "')\n";
                return false;
            }
            opts.timeline_path = arg;
            continue;
        }

        std::string key   = arg.substr(0, eq);
        std::string value = arg.substr(eq + 1);
        if (key == "timeout") {
            try { opts.timeout_seconds = std::stod(value); }
            catch (...) { std::cerr << "Bad timeout= value '" << value << "'\n"; return false; }
        } else if (key == "speed") {
            try { opts.speed = std::stof(value); }
            catch (...) { std::cerr << "Bad speed= value '" << value << "'\n"; return false; }
        } else if (key == "timeline") {
            if (!parse_bool_option(value, opts.auto_timeline)) {
                std::cerr << "Bad timeline= value '" << value << "' (expected true or false)\n";
                return false;
            }
        } else {
            std::cerr << "Unknown option '" << key << "=' (expected timeout=, speed=, or timeline=)\n";
            return false;
        }
    }
    return true;
}

// Compiles and runs a single sketch on the current thread with no GUI,
// streaming Serial output to stdout until Ctrl+C. Mirrors the compile/detect/
// run sequence MainWindow::onRunClicked drives interactively.
static int run_headless(int argc, char* argv[]) {
    std::string sketch_path;
    if (!resolve_sketch_path(argv[1], sketch_path))
        return 1;

    HeadlessOptions opts;
    if (!parse_headless_args(argc, argv, opts))
        return 1;

    if (opts.auto_timeline) {
        if (!opts.timeline_path.empty()) {
            std::cerr << "Can't combine an explicit timeline file with timeline=true\n";
            return 1;
        }
        opts.timeline_path = derive_timeline_path(sketch_path);
        if (!std::filesystem::exists(opts.timeline_path)) {
            std::cerr << "timeline=true but no sibling timeline file found at '"
                       << opts.timeline_path << "'\n";
            return 1;
        }
    }

    std::vector<TimelineEvent> timeline_events;
    if (!opts.timeline_path.empty()) {
        try {
            timeline_events = parse_timeline_file(opts.timeline_path);
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
    }

    QSettings settings = appSettings();
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

    QString boardName = settings.value("board/name", "Arduino Uno").toString();
    BoardProfile profile = boardProfileForName(boardName);

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

    if (!result.board_hint.empty())
        boardProfileForName(QString::fromStdString(result.board_hint), profile);

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
    host.set_speed(opts.speed);

    std::unique_ptr<TestRunner> runner;
    if (!opts.timeline_path.empty())
        runner = std::make_unique<TestRunner>(host, detector.components(), std::move(timeline_events));

    ArduinoRuntime& runtime = host.runtime();
    runtime.on_serial_output  = [&runner](const std::string& t) {
        std::cout << t; std::cout.flush();
        if (runner) runner->onSerialOutput(t);
    };
    runtime.on_serial1_output = [](const std::string& t) { std::cout << t; std::cout.flush(); };
    runtime.on_serial2_output = [](const std::string& t) { std::cout << t; std::cout.flush(); };
    runtime.on_pin_changed = [&runner](int pin, int value) {
        if (runner) runner->onPinChanged(pin, value);
    };
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

    // A timeout can't just be a between-iterations check: a single
    // host.run_loop() call can block for real seconds inside the sketch's
    // own delay()/pulseIn() (e.g. a sketch that does delay(100000) once some
    // finish condition is reached), and that only returns early if
    // stop_requested_ is set *while it's sleeping* -- the same mechanism
    // SIGINT already uses via request_stop(). So timeout=N is implemented
    // the same way: a watchdog thread that calls request_stop() itself
    // rather than a flag this loop merely polls between calls.
    std::mutex              timeout_mtx;
    std::condition_variable timeout_cv;
    bool                    loop_done = false;
    std::thread timeout_thread;
    if (opts.timeout_seconds > 0) {
        timeout_thread = std::thread([&]() {
            std::unique_lock<std::mutex> lock(timeout_mtx);
            bool finished = timeout_cv.wait_for(lock,
                std::chrono::duration<double>(opts.timeout_seconds),
                [&] { return loop_done; });
            if (!finished) {
                g_stop_requested = true;
                runtime.request_stop();
            }
        });
    }

    auto test_start = std::chrono::steady_clock::now();
    while (!g_stop_requested) {
        std::lock_guard<std::mutex> exec_lock(runtime.exec_mutex());
        if (runner) {
            double real_elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - test_start).count();
            runner->fireDueEvents(real_elapsed * opts.speed);
        }
        host.run_loop();
        if (runner && runner->finished()) break;
    }

    if (timeout_thread.joinable()) {
        { std::lock_guard<std::mutex> lock(timeout_mtx); loop_done = true; }
        timeout_cv.notify_one();
        timeout_thread.join();
    }

    if (runner) {
        runner->printSummary();
        return runner->anyAssertFailed() ? 1 : 0;
    }

    std::cout << "\nStopped.\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("VEMCODE");
        app.setApplicationVersion("0.1");
        return run_headless(argc, argv);
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
