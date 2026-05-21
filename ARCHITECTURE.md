# VirtualBench — Architecture Reference

This document is the primary developer reference for VirtualBench. Keep it up to date as the codebase evolves. New contributors and AI assistants should read this before touching any code.

---

## Overview

VirtualBench compiles Arduino sketches to a Windows DLL and runs them against a virtual Arduino runtime. The sketch calls back into the host through a function pointer table. The UI renders component state in real time.

**Platform:** Windows (MinGW), Qt 6.11.1  
**Compiler:** `C:/Qt/Tools/mingw1310_64/bin/g++.exe`  
**Qt path:** `C:/Qt/6.11.1/mingw_64`  
**Exe:** `app/VirtualBench.exe`  
**Repo:** https://github.com/cole-stortz/VirtualBench

---

## Data Flow

```
User writes sketch (.cpp)
    → Preprocessor transforms Arduino syntax → DLL format
    → g++ compiles to sketch.dll
    → SketchHost loads DLL via LoadLibrary
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
VirtualBench/
├── app/                            # Runtime directory
│   ├── VirtualBench.exe
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
│       │   ├── compiler.cpp/h      # Invokes g++, parses errors
│       │   └── preprocessor.cpp/h  # Arduino → VirtualBench source transform
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
    void (*tone)(int, unsigned int, unsigned long);
    void (*noTone)(int);
    // Planned additions:
    // unsigned long (*pulseIn)(int pin, int value, unsigned long timeout);
};

namespace vb { INPUT=0, OUTPUT=1, INPUT_PULLUP=2, LOW=0, HIGH=1 }
constexpr int A0=14, A1=15, A2=16, A3=17, A4=18, A5=19;
```

### RuntimeState (`src/core/runtime/arduinoruntime.h`)

```cpp
struct RuntimeState {
    int  pin_modes[20]    = {};
    int  pin_values[20]   = {};
    int  analog_values[8] = {};
    bool serial_started   = false;
    int  serial_baud      = 0;
    std::chrono::steady_clock::time_point start_time;
};
```

### ComponentInfo (`src/core/circuit/circuitdetector.h`)

```cpp
enum class ComponentType {
    LED, Button, Switch, Buzzer, Servo, Motor,
    Potentiometer, LightSensor, TempSensor, AnalogSensor,
    DistanceSensor, ColorSensor,
    LCD, GenericOutput, GenericInput, Serial
};

struct ComponentInfo {
    ComponentType type;
    std::string   name;
    int           pin;
};
```

---

## Key Classes

### ArduinoRuntime

Owns all simulation state. All `impl_*` functions are static and access state via the global `g_runtime` pointer.

**Key members:**
- `float speed_multiplier_` — scales delay duration. `1.0/speed` where speed is display multiplier
- `std::atomic<bool> stop_requested_` — set by SketchThread to interrupt sleeping delays
- `std::deque<char> serial_buffer_` — bytes injected by UI, consumed by `impl_Serial_read`
- `on_pin_changed`, `on_serial_output`, `on_variable_changed` — callbacks set by SketchHost

**Key methods:**
- `get_api()` — builds and returns the ArduinoAPI struct, sets `g_runtime = this`
- `inject_pin(pin, value)` — sets digital pin state
- `inject_analog(pin, value)` — sets analog value, handles A0-A5 offset (pin 14→index 0)
- `inject_serial(data)` — pushes chars into serial_buffer_
- `set_speed_multiplier(speed)` — sets `speed_multiplier_ = 1.0f / speed`

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

---

### Preprocessor (`src/core/build/preprocessor.cpp`)

Transforms standard Arduino source into VirtualBench DLL format. Steps in order:

1. `is_already_transformed()` — detects `vb_init` or `ArduinoAPI` presence, returns unchanged if found
2. `replace_api_calls()` — string replace all Arduino API calls with `api->` prefixed versions
3. `wrap_functions()` — regex replace `void setup()` / `void loop()` with dllexport decorated versions
4. `inject_safety_delay()` — if no `api->delay(` found, inserts `api->delay(10)` before last `}`
5. `inject_header()` — prepends full header string

**CRITICAL: `INJECTED_HEADER_LINES = 129`**

Must match the exact number of `\n` characters in the injected header string. Used to subtract from compiler error line numbers so errors point to the correct user sketch line. Run the line counter script if the header ever changes:

```python
python3 -c "print(header_string.count('\n'))"
```

**replace_api_calls order matters:**
- `Serial.println` before `Serial.print` (partial match avoidance)
- `digitalRead` before `digitalWrite` (partial match avoidance)
- `delayMicroseconds` before `delay` (partial match avoidance)
- `#include <Servo.h>` stripped (replaced by built-in Servo class in header) — planned

**Injected header contains:**
- `#include "src/core/runtime/arduinoapi.h"`, `<string>`, `<cstring>`
- `using namespace vb`
- `static ArduinoAPI* api = nullptr` + `vb_init`
- Serial overloads (template + const char* + String specializations)
- Full `String` class (wraps std::string)
- `map()`, `constrain()`, `vb_abs/min/max`, `random()`
- `#define abs/min/max` macros
- Planned: `Servo` class, `pulseIn` forwarding

---

### SketchHost (`src/core/host/sketchhost.cpp`)

Manages DLL lifecycle. Uses Windows file timestamp polling for hot-reload detection. Copies sketch.dll to sketch.tmp.dll before loading to avoid Windows file lock.

**Key methods:**
- `load(path)` — copies to .tmp.dll, LoadLibrary, extracts vb_init/vb_setup/vb_loop, calls vb_init
- `reload_if_changed()` — polls file timestamp, reloads if changed
- `inject_pin/inject_analog/inject_serial/set_speed` — delegate to runtime_
- `runtime()` — returns reference to ArduinoRuntime

---

### SketchThread (`src/core/host/sketchhostthread.cpp`)

QThread subclass. Runs vb_loop in a tight loop on a background thread.

**Key methods:**
- `startSketch(path)` — loads sketch, starts thread
- `stopSketch()` — sets `stop_requested_ = true`, then `running_ = false`, then `wait()`
- `injectPin/injectAnalog/injectSerial` — mutex-locked injection
- `setSpeed(float)` — calls host_.set_speed()

**Signals:**
- `serialOutput(QString)`
- `pinChanged(int pin, int value)`
- `variableChanged(QString name, int value)`
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
4. Subtracts `INJECTED_HEADER_LINES` from all line numbers
5. Strips gcc context lines (`\n\s*\d+\s*\|`)
6. Restores `Serial.println(` etc. (preprocessor renamed them)
7. Strips `api->` prefix

**Error highlighting:**
`showCompileErrors()` uses `QTextEdit::ExtraSelection` with `COLOR_ERROR_BG("#3a0000")` to paint error lines red.

**Settings:** Saved to `app/settings.ini` via `QSettings IniFormat`. Keys: `compiler/path`, `compiler/project_root`, `recent/sketches`.

**Recent sketches:** Last 5 paths, stored as QStringList. `addToRecentSketches(path)` called on open/save/new.

**Serial monitor:** Read-only output + input row (QLineEdit + Send button). `onSerialSend()` injects text+"\n" and echoes "> text" in monitor.

---

### CircuitDetector (`src/core/circuit/circuitdetector.cpp`)

Two-phase detection:

**Phase 1 — `#define` keywords:**
Scans for `#define XXX_PIN N` patterns and matches the name against keyword lists.

**Phase 2 — `analogRead` calls:**
Scans for `analogRead(A0)` etc. to detect analog sensor components not caught by phase 1.

**Planned Phase 3 — array declarations:**
Scan for `const int NAME[N] = {...}` patterns for color sensor and multi-pin array-based components.

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

```powershell
# Configure (all one line)
cmake -B build -S . -G "Ninja" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe"

# Build
cmake --build build

# Deploy Qt runtime (first time only)
C:\Qt\6.11.1\mingw_64\bin\windeployqt.exe .\app\VirtualBench.exe
```

---

## Known Gotchas

- **`INJECTED_HEADER_LINES = 129`** — must match header `\n` count exactly. Run python counter if header changes.
- **`inject_analog` and `impl_analogRead`** — both must apply `pin >= 14 ? pin-14 : pin` offset.
- **`dragPin_`, `dragStartY_`, `dragStartValue_`** — must be in private section of canvaswidget.h.
- **`runtime()` accessor** — already exists in sketchhost.h, don't add a duplicate.
- **Windows file lock** — always load `.tmp.dll` copy, never `sketch.dll` directly.
- **SketchThread callbacks** — fire on background thread, only emit Qt signals from them.
- **String literals left operand** — `"literal" + String(x)` fails. Use `String("literal") + x`.
- **`stopSketch()` order** — sets `stop_requested_` BEFORE `running_=false` then `wait()`.
- **Preprocessor replace order** — println before print, digitalRead before digitalWrite, delayMicroseconds before delay.
- **Raw string header** — if ever switching back to raw string R"(...)", all `#include`, `#define`, `using`, `class`, `inline` must start at column 0 — no indentation.
- **`#include <Servo.h>`** — must be stripped in preprocessor, replaced by built-in Servo class in injected header.
- **Motor vs Servo** — MOTOR keyword → Motor (H-bridge), SRV/SERVO → Servo (PWM). These are different component types with different pin counts and behaviors.
- **Array pin declarations** — `const int PIN[N] = {...}` needs a separate detection pass in CircuitDetector, not just `#define` patterns.
- **Pin count** — RuntimeState arrays are `[20]`, all sketches must remap pins to fit 0-19. If Teensy or Mega support added later, expand arrays accordingly.

---

## Target Benchmark Sketch

The simplified Lambo robot sketch is the primary milestone target for Phase 1 completion. When this sketch compiles and runs correctly, Phase 1 is done.

**Components:**
- 1x TCS3200 color sensor (S0-S3 + OUT, pins 14-18)
- 1x HC-SR04 ultrasonic sensor (TRIG pin 12, ECHO pin 13)
- 3x H-bridge DC motors (3 pins each: PWM + CW + ACW, pins 3-11)
- 1x Servo arm (pin 2)

**Total: 17 pins, all within 0-19**

**What it exercises:**
- `pulseIn()` — ultrasonic duration + color sensor frequency
- `delayMicroseconds()` — ultrasonic trigger pulse
- `analogWrite()` — motor PWM speed control
- `digitalWrite/digitalRead` — motor direction, color sensor S-pins
- `Servo.attach/write` — arm positioning
- Array-based pin access — `S0[i]`, `sensorOut[i]`
- Multi-pin component grouping — color sensor arrays, motor H-bridge groups

---

## Roadmap

### Phase 1 — Component Completion (in progress)
- `pulseIn(pin, value)` and `pulseIn(pin, value, timeout)`
- `delayMicroseconds` as scaled milliseconds
- Servo class in injected header + `#include <Servo.h>` stripping in preprocessor
- `analogWrite` fires `on_pin_changed` for signal timeline tracking
- Array-based pin detection (`const int PIN[N] = {...}`)
- Multi-pin component grouping (HC-SR04, H-bridge motor, color sensor)
- Motor (H-bridge 4-pin) separated from Servo (PWM single pin)

> **Milestone:** Target benchmark sketch compiles and runs correctly.

### Phase 2 — Component Visuals
- Proper graphics for all component types replacing colored rectangles
- Per-component sensor input boxes (type distance in cm, temperature in °C, color picker, etc.)

### Phase 3 — Canvas Improvements
- Canvas layout mode — "Layout" toolbar button, components become draggable
- Positions saved to `sketch_name.vblayout` next to `.cpp` file
- On load: use saved positions if file exists, otherwise auto-generate
- Wire visualization improvements — color coded by signal type

### Phase 4 — Simulation Realism
- Floating pin simulation — undriven INPUT pins return random HIGH/LOW
- Button bounce simulation — rapid toggles on click before settling (~10ms)
- Optional gaussian noise on analog readings (off by default)

### Phase 5 — New Arduino Features
- `attachInterrupt(pin, ISR, mode)` — RISING, FALLING, CHANGE
- EEPROM simulation — 1024 bytes, optional disk persistence between sessions
- Basic I2C simulation (`Wire.begin`, `Wire.write`, `Wire.read`)
- Basic SPI simulation (`SPI.begin`, `SPI.transfer`)

### Phase 6 — Display Support
- 16x2 LCD — `LiquidCrystal` compatible, renders actual characters on canvas
- 7-segment display — single and multi-digit
- Basic OLED — text and simple graphics

### Phase 7 — Multi-board Simulation
- Two SketchThread instances running simultaneously
- Virtual serial pipe connecting them (TX of one → RX of other)
- Enables master/slave and sensor node + controller patterns

### Phase 8 — Memory Analysis
- `avr_gcc_path` in settings dialog
- After successful Windows compile, run `avr-gcc` compile for size analysis only
- Parse `avr-size` output for flash and RAM usage
- Flash → hard enforce, block Run if over 32,256 bytes
- Static RAM → hard enforce, block Run if globals exceed 2,048 bytes
- Dynamic RAM (String/malloc) → warn but don't block
- Memory bar in UI: `████░░░░ 1234 / 32256 bytes (3%)`
- Auto-detect Arduino IDE avr-gcc path on first run
- Warn at >75% usage before hitting limit

### Later
- macOS / Linux support
- Installer — bundle MinGW for zero-dependency install
- Additional board support (Nano, Mega, ESP32)

### Explicitly Out of Scope
- Box2D / physics simulation
- 3D visualization
- External robot simulator integration (Gazebo, Webots)

---

## Permanent Limitations

- Microsecond-accurate timing (Windows scheduler ~1-15ms jitter)
- Hardware protocol electrical behavior (I2C/SPI bus capacitance, voltage levels)
- Register-level / AVR assembly programming
- Real electrical behavior (voltage, current, short circuits)
- Dynamic RAM enforcement is approximate — static RAM hard enforced via avr-gcc, heap from String/malloc tracked with warnings
- Libraries that wrap AVR hardware registers directly