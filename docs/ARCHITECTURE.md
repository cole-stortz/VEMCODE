# VEMCODE — Architecture Reference

This document is the primary developer reference for VEMCODE. Keep it up to date as the codebase evolves. New contributors and AI assistants should read this before touching any code.

---

## Overview

VEMCODE compiles embedded sketches to a native shared library and runs them against a virtual runtime. The sketch calls back into the host through a function pointer table. The UI renders component state in real time. Currently supports Arduino and Teensy boards (C++); MicroPython and CircuitPython boards are planned via a Python execution path (see Phase 14 in ROADMAP.md).

**Platform:** Windows (MinGW) and Linux, Qt 6.x  
**Windows compiler:** `C:/Qt/Tools/mingw1310_64/bin/g++.exe`  
**Windows Qt path:** `C:/Qt/6.11.1/mingw_64`  
**Linux compiler:** `/usr/bin/g++`  
**Exe:** `app/VEMCODE.exe` (Windows), `app/VEMCODE` (Linux)  
**Repo:** https://github.com/cole-stortz/VEMCODE

---

## Data Flow

```
User writes sketch (.cpp)
    → Preprocessor transforms Arduino syntax → shared library format
    → g++ compiles to sketch.so (Linux) / sketch.dll (Windows)
    → SketchHost loads library via dlopen (Linux) / LoadLibrary (Windows)
    → SketchHost calls vb_init(&api) to inject function pointer table
    → SketchThread calls vb_setup() once, then vb_loop() in a loop
    → Sketch calls api->digitalWrite() etc.
    → ArduinoRuntime implements all calls, fires callbacks
    → Callbacks emit Qt signals on SketchThread
    → MainWindow slots update canvas, serial monitor, timeline, variable watch
```

---

## File Structure

```
VEMCODE/
├── app/                            # Runtime directory
│   ├── VEMCODE.exe
│   ├── sketches/                   # User sketches (each in own subfolder)
│   └── settings.ini                # QSettings — compiler path, recent sketches (gitignored)
├── src/
│   ├── main.cpp
│   ├── ui/
│   │   ├── mainwindow.cpp/h        # Main window, toolbar, all slot wiring
│   │   ├── canvaswidget.cpp/h      # Circuit canvas, component rendering, interaction
│   │   ├── signaltimeline.cpp/h    # Logic analyzer waveform view
│   │   ├── codehighlighter.cpp/h   # VS Code style syntax highlighting
│   │   ├── linenumberarea.cpp/h    # Line number gutter (EditorWithLines subclass trick)
│   │   ├── variablewatch.cpp/h     # QTableWidget for watch_variable values
│   │   └── settingsdialog.cpp/h    # First-run and settings dialog
│   └── core/
│       ├── runtime/
│       │   ├── arduinoapi.h        # ArduinoAPI struct (function pointer table)
│       │   └── arduinoruntime.cpp/h # All impl_* functions, runtime state
│       ├── host/
│       │   ├── sketchhost.cpp/h        # LoadLibrary, hot-reload, DLL lifecycle
│       │   └── sketchhostthread.cpp/h  # QThread wrapper, inject methods, signals
│       ├── build/
│       │   ├── compiler.cpp/h          # Invokes g++, parses errors
│       │   ├── preprocessor.cpp/h      # Arduino → VEMCODE source transform
│       │   ├── injected_header.inc       # Header injected above every sketch (edit this to add API)
│       │   └── injected_header.cpp.in  # CMake template — wraps injected_header.inc as a C string
│       └── circuit/
│           └── circuitdetector.cpp/h   # Keyword-based component detection
├── sketches/                       # Example/test sketches
├── ARCHITECTURE.md                 # This file
├── README.md
└── CMakeLists.txt
```

---

## Core Structs

### ArduinoAPI (`src/core/runtime/arduinoapi.h`)

Function pointer table injected into the sketch DLL via `vb_init`. Every Arduino API call goes through this table.

```cpp
struct ArduinoAPI {
    void (*pinMode)(int, int);
    void (*digitalWrite)(int, int);
    int  (*digitalRead)(int);
    void (*analogWrite)(int, int);
    int  (*analogRead)(int);
    void (*delay)(unsigned long);
    void (*delayMicroseconds)(unsigned long);
    unsigned long (*millis)();
    unsigned long (*micros)();
    void (*Serial_begin)(int);
    void (*Serial_print)(const char*);
    void (*Serial_println)(const char*);
    void (*watch_variable)(const char*, int);
    int  (*Serial_available)();
    int  (*Serial_read)();
    unsigned long (*pulseIn)(int pin, int value, unsigned long timeout);
    // Note: tone/noTone are in preprocessor replace list but not yet implemented in runtime
    void (*lcd_print)(int pin, int row, const char* text);
};

namespace vb { INPUT=0, OUTPUT=1, INPUT_PULLUP=2, LOW=0, HIGH=1 }
constexpr int A0=14, A1=15, A2=16, A3=17, A4=18, A5=19;
```

### RuntimeState (`src/core/runtime/arduinoruntime.h`)

```cpp
struct RuntimeState {
    int  pin_modes[80]    = {};
    int  pin_values[80]   = {};  // digital pin state (LOW/HIGH)
    int  analog_values[20] = {}; // injected analog values, index = pin - analog_offset
    int  pwm_values[80]   = {};  // analogWrite values (0-255 or 0-4095), used for servo angle
    bool serial_started   = false;
    int  serial_baud      = 0;
    unsigned long pulse_durations_[20] = {};  // pre-set pulseIn return values (µs), e.g. distance sensor
    std::map<int, std::array<unsigned long, 4>> color_channels_; // out_pin → [R,Blue,Clear,G] periods
    std::map<int, int> color_sensor_s2_; // out_pin → s2_pin (for channel lookup)
    std::map<int, int> color_sensor_s3_; // out_pin → s3_pin (for channel lookup)
    std::chrono::steady_clock::time_point start_time;
};
```

### DetectedComponent (`src/core/circuit/circuitdetector.h`)

```cpp
enum class ComponentType {
    LED, Button, Switch, Buzzer, Servo,
    Potentiometer, LightSensor, TempSensor, AnalogSensor,
    LCD, GenericOutput, GenericInput, Serial,
    DistanceSensor,   // HC-SR04: TRIG + ECHO
    HBridgeMotor,     // 3-pin motor: PWM + CWISE + ANTI_CWISE
    ColorSensor       // TCS3200: S0 + S1 + S2 + S3 + OUT
};

struct DetectedComponent {
    ComponentType    type;
    int              pin;          // primary pin (-1 for Serial)
    std::vector<int> pins;         // all pins for multi-pin components
    std::string      pin_name;     // original #define name e.g. "LED_PIN"
    std::string      label;        // human readable e.g. "LED (pin 13)"
    bool             confirmed;    // true once runtime pin_changed fires for this pin
};
```

### BoardProfile (`src/core/runtime/boardprofile.h`)

Describes the hardware characteristics of a supported board. Selected in Settings, passed to the runtime and canvas at startup. Adding a new board means adding one entry here — nothing else changes.

```cpp
// Phase 14 will add: enum class Language { Arduino, MicroPython, CircuitPython };
// Preprocessor checks profile.language to pick the replacement table and execution path.

struct BoardProfile {
    const char* name;           // "Arduino Uno", "Teensy 4.1", "Raspberry Pi Pico", ...
    const char* chip;           // "ATmega328P", "IMXRT1062", "RP2040", ...
    int         pin_count;      // total pins
    int         analog_offset;  // pin number where A0 starts
    int         analog_count;   // number of analog pins
    int         pwm_resolution; // max analogWrite value (255 or 4095)
    // Language    language;    // planned — Arduino (default), MicroPython, CircuitPython
};

// Built-in profiles
static const BoardProfile BOARD_UNO    = {"Arduino Uno",       "ATmega328P",   20, 14,  6,  255};
static const BoardProfile BOARD_NANO   = {"Arduino Nano",      "ATmega328P",   22, 14,  8,  255};
static const BoardProfile BOARD_MEGA   = {"Arduino Mega 2560", "ATmega2560",   70, 54, 16,  255};
static const BoardProfile BOARD_DUE    = {"Arduino Due",       "AT91SAM3X8E",  66, 54, 12, 4095};
static const BoardProfile BOARD_TEENSY = {"Teensy 4.1",        "IMXRT1062",    42, 14, 18, 4095};
```

**What consumes the profile:**
- `RuntimeState` — `pin_count` replaces hardcoded `[20]`
- `inject_analog` / `impl_analogRead` — `analog_offset` replaces hardcoded `14`
- `CanvasWidget` — pin loop and board graphic height driven by `pin_count`
- `CircuitDetector` — valid pin range check uses `pin_count`
- MainWindow toolbar — displays `profile.name`
- Settings dialog — board selector, saved to `settings.ini`

**Board hint override (`// @board <name>`):**
A sketch can declare its target board by placing a `// @board <name>` comment anywhere near the top of the file (before the first function definition in practice). When the user hits Run, `Preprocessor::extract_board_profile()` scans the raw source for this comment and returns the board name. `MainWindow` resolves it to a `BoardProfile`, then calls the same three-call update chain used by the Settings dialog: `canvasWidget_->setProfile()`, `boardLabel_->setText()`, and `sketchThread_->setProfile()`. The resolved board name is also written back to `settings.ini` so it persists.

---

## Key Classes

### ArduinoRuntime

Owns all simulation state. All `impl_*` functions are static and access state via the global `g_runtime` pointer.

**Key members:**
- `float speed_multiplier_` — scales delay duration. `1.0/speed` where speed is display multiplier
- `std::atomic<bool> stop_requested_` — set by SketchThread to interrupt sleeping delays
- `std::deque<char> serial_buffer_` — bytes injected by UI, consumed by `impl_Serial_read`
- `on_pin_changed`, `on_serial_output`, `on_variable_changed`, `on_lcd_print` — callbacks set by SketchHost; fire on background thread, only emit Qt signals from them

**Key methods:**
- `get_api()` — builds and returns the ArduinoAPI struct, sets `g_runtime = this`
- `inject_pin(pin, value)` — sets digital pin state
- `inject_analog(pin, value)` — sets analog value, handles A0-A5 offset (pin 14→index 0)
- `inject_serial(data)` — pushes chars into serial_buffer_
- `set_speed_multiplier(speed)` — sets `speed_multiplier_ = 1.0f / speed`
- `inject_pulse_duration(pin, micros)` — pre-sets the pulseIn fast-path return value for a pin (used by DistanceSensor canvas input)
- `inject_color(out_pin, s2_pin, s3_pin, r, g, b)` — converts R/G/B 0-255 to TCS3200 frequency periods and stores all 4 channels keyed by out_pin

**impl_delay:**
Sleeps in 10ms chunks, checks `stop_requested_` between each chunk. Critical — without chunking, Stop blocks until the full delay completes.

```cpp
void ArduinoRuntime::impl_delay(unsigned long ms) {
    if (!g_runtime) return;
    unsigned long scaled = (unsigned long)(ms * g_runtime->speed_multiplier_);
    unsigned long elapsed = 0;
    while (elapsed < scaled) {
        if (g_runtime->stop_requested_) return;
        unsigned long chunk = std::min(10UL, scaled - elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        elapsed += chunk;
    }
}
```

**impl_delayMicroseconds:**
Busy-waits in 50µs chunks, checks `stop_requested_` between each chunk. Not real µs accuracy — simulated only. Fine for HC-SR04's short pulses (2µs trigger, 10µs activation).

**impl_pulseIn — three-path implementation:**
1. **Fast path:** if `pulse_durations_[pin] > 0`, return it immediately (used by distance sensor canvas input)
2. **Color path:** if pin is a registered TCS3200 OUT pin, look up current S2/S3 pin states and return the matching channel period
3. **Slow path:** polls pin state in a loop — waits for pin to enter `value` state then times it in µs (works for simple simulated toggles)

---

### Preprocessor (`src/core/build/preprocessor.cpp`)

Transforms standard Arduino source into VEMCODE DLL format.

**`extract_board_profile(source)`** — called by `Compiler::compile()` on the raw source *before* `process()`. Scans the first occurrence of `// @board <name>` using regex `//\s*@board\s+(.+)`, trims trailing whitespace, and returns the board name string (empty if not found). The result is surfaced in `CompileResult::board_hint` and applied by MainWindow after compilation.

**`process()` steps in order:**

1. `is_already_transformed()` — detects `vb_init` or `ArduinoAPI` presence, returns unchanged if found
2. `replace_api_calls()` — string replace all Arduino API calls with `api->` prefixed versions
3. `wrap_functions()` — regex replace `void setup()` / `void loop()` with dllexport decorated versions
4. `inject_safety_delay()` — if no `api->delay(` found, inserts `api->delay(10)` before last `}`
5. `inject_header()` — prepends full header string

**Injected header line count** is computed dynamically at runtime by counting `\n` characters in `g_injected_header`. No manual constant to update when the header changes.

**replace_api_calls order matters:**
- `Serial.println` before `Serial.print` (partial match avoidance)
- `digitalRead` before `digitalWrite` (partial match avoidance)
- `delayMicroseconds` before `delay` (partial match avoidance)
- `pulseIn(` → `api->pulseIn(` (all 3 args required; no default-arg wrapper in injected header)
- `tone`/`noTone` — replaced but not yet implemented in runtime
- `#include <Servo.h>` and `#include <LiquidCrystal.h>` — stripped and replaced by built-in stub classes injected inline at the `#include` site by `strip_includes()`

**Injected header** lives in `src/core/build/injected_header.inc` — edit that file directly to add or change API surface. CMake reads it at configure time and embeds it as `g_injected_header` via `injected_header.cpp.in`. `inject_header()` just prepends `g_injected_header` to the source.

**Injected header contains:**
- `#include "src/core/runtime/arduinoapi.h"`, `<string>`, `<cstring>`, `<sstream>`, `<cstdlib>`, `<cstdint>`, `<cmath>`, `<climits>`
- `using namespace vb`
- `static ArduinoAPI* api = nullptr` + `vb_init`
- Serial overloads: template (any type), `const char*`, `char`, `String` specializations
- Serial format overloads: `Serial_print(T, int base)` — HEX/BIN/OCT/DEC via `HEX=16`, `BIN=2`, `OCT=8`, `DEC=10` constants
- Serial float precision: `Serial_print(float, int decimals)` — uses `std::fixed` + `std::setprecision`
- Full `String` class (wraps std::string)
- `map()`, `constrain()`, `vb_abs/min/max`, `random()`, `randomSeed()`
- `#define abs/min/max` macros
- `#define PROGMEM` and `#define F(x) (x)` — flash-string helpers compile without modification
- `pulseIn` inline wrapper
- `Servo` class — injected when `#include <Servo.h>` is stripped by `strip_includes()`

---

### SketchHost (`src/core/host/sketchhost.cpp`)

Manages shared library lifecycle. Uses file timestamp polling for hot-reload detection. Copies the library to a temp file before loading (`.tmp.dll` / `.tmp.so` / `.tmp.dylib`) — on Windows this is required to avoid a file lock; on Linux it is harmless and keeps behaviour consistent.

**Key methods:**
- `load(path)` — copies to temp file, loads via `LoadLibrary`/`dlopen`, extracts vb_init/vb_setup/vb_loop, calls vb_init
- `reload_if_changed()` — polls file timestamp, reloads if changed
- `inject_pin/inject_analog/inject_serial/set_speed` — delegate to runtime_
- `inject_pulse_duration(pin, micros)` — inline, delegates to `runtime_.inject_pulse_duration`
- `inject_color(out_pin, s2_pin, s3_pin, r, g, b)` — inline, delegates to `runtime_.inject_color`
- `runtime()` — returns reference to ArduinoRuntime

---

### SketchThread (`src/core/host/sketchhostthread.cpp`)

QThread subclass. Runs vb_loop in a tight loop on a background thread.

**Key methods:**
- `startSketch(path)` — loads sketch, starts thread
- `stopSketch()` — sets `stop_requested_ = true`, then `running_ = false`, then `wait()`
- `injectPin/injectAnalog/injectSerial` — mutex-locked injection
- `injectPulseDuration(pin, micros)` — mutex-locked, delegates to host
- `injectColor(out_pin, s2_pin, s3_pin, r, g, b)` — mutex-locked, delegates to host
- `setSpeed(float)` — calls host_.set_speed()

**Signals:**
- `serialOutput(QString)`
- `pinChanged(int pin, int value)`
- `variableChanged(QString name, int value)`
- `lcdPrint(int pin, int row, QString text)` — emitted from `on_lcd_print` callback; connected to `CanvasWidget::updateLcdText`
- `sketchReloaded()`
- `loadFailed(QString reason)`

**Thread safety:** All inject methods use `inject_mutex_`. Callbacks fire on background thread — only emit Qt signals from them, never touch UI directly.

---

### CanvasWidget (`src/ui/canvaswidget.cpp`)

QPainter-based canvas. Repaints on `updatePin` calls.

**Layout:**
- Arduino board drawn in center-right area
- Output components (LED, Buzzer, Servo, Motor, GenericOutput): right side, Y aligned to pin position
- Digital inputs (Button, Switch, Potentiometer, GenericInput): left inner column (`BOARD_X - comp_w - 80`)
- Analog sensors (LightSensor, TempSensor, AnalogSensor, DistanceSensor, ColorSensor): left outer column (`BOARD_X - comp_w - 180`)
- Wire routing: sensors use 3-segment wire through inner column edge

**Interaction:**
- Button: mousePress=LOW (INPUT_PULLUP logic), mouseRelease=HIGH → emits `buttonPressed(pin, value)`
- Switch: mousePress toggles state, persists in `switchStates_` QMap → emits `buttonPressed(pin, value)`
- Potentiometer: drag up/down changes value 0-1023 → emits `potentiometerChanged(pin, value)`
  - `dragPin_`, `dragStartY_`, `dragStartValue_` track drag state

**Canvas Inputs (QGraphicsProxyWidget + QLineEdit):**
- DistanceSensor: one QLineEdit (cm), default "10". On change emits `pulseInjected(pin, (unsigned long)ceil(cm * 2.0f / 0.034f))`
- LightSensor / TempSensor / AnalogSensor: one QLineEdit (0-1023). On change emits `analogInjected(pin, value)`
- ColorSensor: three QLineEdit fields (R/G/B 0-255). On any change emits `colorInjected(out_pin, s2_pin, s3_pin, r, g, b)`

**Critical:** connect signal THEN call `input->setText("10")`. `textChanged` does NOT fire from the constructor, so the initial injection only happens if `setText` is called after the connection is made.

**Multi-wire routing:**
Multi-pin components draw one wire per pin. Each wire is L-shaped: horizontal to the board, then vertical clamped to `qBound(comp_y, pin_y, comp_y + comp_h)` so each wire clearly shows which pin it connects to.

**Servo angle display:**
`servoLabels_` QMap stores a `QGraphicsTextItem*` per servo pin. Created in `drawComponent`, updated in `updatePin` when `analogWrite` fires: `angle = value * 180 / 255`, label text is `"°" + angle`.

**LCD text display:**
`lcdRow0Labels_` and `lcdRow1Labels_` QMaps store a `QGraphicsTextItem*` per row, keyed by RS pin number. Created as children of the LCD `QGraphicsRectItem` in `drawComponent` (so they move with the box and don't need explicit scene add). Updated via `updateLcdText(pin, row, text)` — the `CanvasWidget` slot connected to `SketchThread::lcdPrint`. Text is clamped to 16 characters via `QString::left(16).leftJustified(16)`. Cleared in `refresh()` before each redraw.

**Analog index conversion:**
Both `inject_analog` (CanvasWidget → SketchThread) and `impl_analogRead` (runtime) apply:
```cpp
int analog_index = (pin >= 14) ? pin - 14 : pin;
```

---

### MainWindow (`src/ui/mainwindow.cpp`)

**Toolbar:** Run, Stop, Speed slider (1-25, value/10 = display speed), New Sketch, Open, Recent, Save, Settings, Board label

**Speed slider:**
- Range 1-25, default 10 (= 1.0x)
- `display_speed = value / 10.0f`
- Passed to `sketchThread_->setSpeed(display_speed)`
- Runtime: `speed_multiplier_ = 1.0f / speed`

**Compile error flow:**
1. Compiler writes temp file `sketch_dir/_vb_temp.cpp`
2. Error output contains temp file path + raw line numbers
3. MainWindow regex-replaces path with `line N:`
4. Subtracts `preprocessor.injectedLines()` from all line numbers (dynamic — counts `\n` in header + forward declarations)
5. Strips gcc context lines (`\n\s*\d+\s*\|`)
6. Restores `Serial.println(` etc. (preprocessor renamed them)
7. Strips `api->` prefix

**Error highlighting:**
`showCompileErrors()` uses `QTextEdit::ExtraSelection` with `COLOR_ERROR_BG("#3a0000")` to paint error lines red.

**Board hint application:** After `compiler.compile()` returns, if `result.board_hint` is non-empty, MainWindow resolves it to a `BoardProfile` and calls `canvasWidget_->setProfile()`, `boardLabel_->setText()`, and `sketchThread_->setProfile()` — the same three calls the Settings dialog makes on save. The board name is also persisted to `settings.ini` via `QSettings` so it survives a restart.

**Settings:** Saved to `app/settings.ini` via `QSettings IniFormat`. Keys: `compiler/path`, `compiler/project_root`, `recent/sketches`, `board/name`.

**Recent sketches:** Last 5 paths, stored as QStringList. `addToRecentSketches(path)` called on open/save/new.

**Serial monitor:** Read-only output + input row (QLineEdit + Send button). `onSerialSend()` injects text+"\n" and echoes "> text" in monitor.

---

### CircuitDetector (`src/core/circuit/circuitdetector.cpp`)

Four-phase detection. `detect()` runs all phases in order; phases 1-2 skip pins already claimed by phase 0.

**Parse step — `parse_arrays()`:**
Runs before all detection phases. Regex-scans for `const int NAME[N] = {v1, v2, ...}` and builds `arrays_` map (name → `vector<int>`). Used by phase 0 to resolve array-based pin lists.

**Phase 0 — Multi-pin components (`detect_multipin`):**
Claims pins before single-pin phases so components aren't double-detected.
- HC-SR04: `_TRIG` / `_ECHO` define pairs grouped by common prefix → `DistanceSensor`
- H-bridge: defines sharing a `find('_')` prefix (e.g. MOTOR1_CW + MOTOR1_ANTI + MOTOR1_PWM) → `HBridgeMotor`
- TCS3200: S0/S1/S2/S3/sensorOut names from `arrays_` map → `ColorSensor`
- LCD: RS + E + D4-D7 define groups → `LCD`

**Phase 1 — `#define` keywords:**
Scans for `#define XXX_PIN N` patterns and matches the name against keyword lists. Skips claimed pins.

**Phase 2 — `analogRead` calls:**
Scans for `analogRead(A0)` etc. to detect analog sensor components not caught by phases 0-1. Skips claimed pins.

**`confirm_pin(pin)`:** Called from `onPinChanged` when a pin actually fires. Promotes tentative detections to confirmed.

**Detection keywords:**
```
LED:           LED, LIGHT, LAMP, INDICATOR
Button:        BTN, BUTTON, KEY
Switch:        SWITCH, SW, TOGGLE
Buzzer:        BUZZER, BUZZ, SPEAKER, TONE, PIEZO
Servo:         SERVO, SRV                          (single PWM pin)
Motor:         MOTOR, CW, CWISE, ANTI, IN1, IN2    (H-bridge, groups of 3-4 pins)
Pot:           POT, POTENTIOMETER, DIAL
LightSensor:   PHOTO, LDR, LIGHT_SENSOR, PHOTORESISTOR
TempSensor:    TEMP, TEMPERATURE, THERMISTOR
AnalogSensor:  SENSOR, ANALOG, ADC
DistanceSensor: TRIG, ECHO                         (multi-pin pair)
ColorSensor:   S0, S1, S2, S3, COLOR_OUT           (multi-pin group)
LCD:           LCD, DISPLAY, SCREEN, OLED
```

**Motor vs Servo distinction:**
- `SERVO`, `SRV` → Servo (single PWM pin, angle from analogWrite)
- `MOTOR`, `CW`, `CWISE`, `ANTI`, `IN1`-`IN4` → Motor (H-bridge, direction from digitalWrite pairs)

---

## Editor Features (`src/ui/mainwindow.cpp — eventFilter`)

- **Tab:** inserts 4 spaces
- **Enter:** carries forward current line indentation, adds 4 spaces if line ends with `{`
- **`}`:** if current line is only whitespace, removes one indent level then inserts `}`
- **Ctrl+S:** saves sketch
- **Syntax highlighting:** `CodeHighlighter` — blue keywords, yellow functions, green comments, orange strings
- **Line numbers:** `EditorWithLines` + `LineNumberArea` subclass trick

---

## Build Commands

**Windows:**

```powershell
# Configure (all one line)
cmake -B build -S . -G "Ninja" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe"

# Build
cmake --build build

# Deploy Qt runtime (first time only)
C:\Qt\6.11.1\mingw_64\bin\windeployqt.exe .\app\VEMCODE.exe

# Run
.\app\VEMCODE.exe
```

**Linux:**

```bash
# Configure
cmake -B build -S .

# Build (incremental)
cmake --build build

# Run
./app/VEMCODE
```

---

## Known Gotchas

- **`inject_analog` and `impl_analogRead`** — both must apply `pin >= 14 ? pin-14 : pin` offset.
- **`dragPin_`, `dragStartY_`, `dragStartValue_`** — must be in private section of canvaswidget.h.
- **`runtime()` accessor** — already exists in sketchhost.h, don't add a duplicate.
- **Windows file lock** — always load the `.tmp.dll`/`.tmp.so` copy, never the original directly. On Windows, loading the original locks it and the compiler can't overwrite it. On Linux the copy is harmless but keeps behaviour consistent.
- **SketchThread callbacks** — fire on background thread, only emit Qt signals from them.
- **String literals left operand** — `"literal" + String(x)` fails. Use `String("literal") + x`.
- **`stopSketch()` order** — sets `stop_requested_` BEFORE `running_=false` then `wait()`.
- **Preprocessor replace order** — println before print, digitalRead before digitalWrite, delayMicroseconds before delay.
- **`injected_header.inc` changes require cmake re-run** — `set_property(CMAKE_CONFIGURE_DEPENDS)` makes the build system detect the change automatically, but only if you build via cmake (`cmake --build build`), not by invoking g++ directly.
- **`#include <Servo.h>` and `#include <LiquidCrystal.h>`** — must be stripped in preprocessor and replaced by built-in stub classes. `strip_includes()` handles both; LiquidCrystal before Servo in the replacement order.
- **`LiquidCrystal` global constructor safety** — `LiquidCrystal lcd(rs, en, d4, d5, d6, d7)` is typically declared at global scope and its constructor runs before `vb_init`. The stub constructor only stores `rs_` as an int; it never calls `api->`. Safe — `api` is only called in `begin()`, `print()`, `clear()`, and `setCursor()`, which run after `vb_init`.
- **LCD text items are children of the rect, not the scene** — `QGraphicsTextItem` for LCD rows is parented to the `QGraphicsRectItem` (not added via `scene_->addItem()`). Double-adding a child item to the scene causes a crash. Creating it as a child of the rect is sufficient.
- **LCD RS pin as representative** — `CanvasWidget` maps LCD text items by RS pin number. Only the RS pin is registered in `pinItems_`; the other 5 LCD pins are not tracked individually.
- **Motor vs Servo** — MOTOR keyword → Motor (H-bridge), SRV/SERVO → Servo (PWM). These are different component types with different pin counts and behaviors.
- **`// @board` hint matching** — the board name in the comment must exactly match a `BoardProfile.name` string (e.g. `"Teensy 4.1"`, `"Arduino Uno"`). An unrecognized name silently falls back to the settings board — no error is shown.
- **Pin count** — RuntimeState arrays sized by `BoardProfile.pin_count`. Default is Uno (20). Sketches targeting Teensy or Mega must select the matching board in Settings or use a `// @board` comment.
- **Analog offset** — `inject_analog` and `impl_analogRead` will use `BoardProfile.analog_offset` instead of hardcoded `14`. When adding a board, set `analog_offset` correctly or `analogRead(A0)` returns wrong values.
- **PWM resolution** — `BoardProfile.pwm_resolution` is 255 for Uno, 4095 for Teensy/STM32. Canvas servo angle calculation (`value * 180 / pwm_resolution`) must use the profile value, not a hardcoded 255.
- **`pulseIn` requires all 3 args** — the function pointer has no default. Sketches calling `pulseIn(PIN, HIGH)` must use `pulseIn(PIN, HIGH, 1000000)`.
- **TCS3200 channel encoding** — channel index = `s2_val * 2 + s3_val`. Stored as `[0=Red, 1=Blue, 2=Clear, 3=Green]` — NOT intuitive R/G/B/Clear order.
- **H-bridge prefix uses `find('_')` not `rfind('_')`** — `MOTOR1_ANTI_CWISE` must yield prefix `MOTOR1`. Using `rfind` gives `MOTOR1_ANTI` and breaks motor grouping.
- **QLineEdit initial value** — `textChanged` does NOT fire from constructor. Connect the signal handler THEN call `setText("10")` to trigger initial injection.
- **Distance conversion** — use `(unsigned long)std::ceil(cm * 2.0f / 0.034f)`, not `cm * 58.2f`. The ceil prevents floor truncation from the integer cast.

---

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the full phase-by-phase plan.

---

## Limitations

**Permanent (require replacing the compile-to-native-DLL architecture):**
- AVR assembly instructions (`asm volatile`) — CPU instructions don't exist on x86
- Hardware interrupt vectors (`ISR(TIMER1_OVF_vect)` etc.) — the interrupt hardware doesn't exist
- Real electrical behavior (voltage, current, short circuits) — requires SPICE-level simulation
- `int` is 32-bit on x86 vs 16-bit on AVR — sketches that rely on `int` wrapping at 32767 will behave differently in simulation; use fixed-width types (`int16_t`, `uint16_t`, `uint8_t`, etc.) for exact-width behavior. All `<cstdint>` types work correctly and are available via the injected header.