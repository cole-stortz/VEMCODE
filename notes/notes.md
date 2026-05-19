# VirtualBench — Architecture & Developer Reference

## Table of contents

- [Project structure](#project-structure)
- [How the simulation works](#how-the-simulation-works)
- [Adding a new Arduino API function](#adding-a-new-arduino-api-function)
- [Adding a new component](#adding-a-new-component)
- [Adding a new Arduino board](#adding-a-new-arduino-board)
- [Thread safety rules](#thread-safety-rules)
- [Hot-reload gotchas](#hot-reload-gotchas)
- [Preprocessor line number offset](#preprocessor-line-number-offset)
- [Circuit detection ordering](#circuit-detection-ordering)
- [CMakeLists.txt checklist](#cmakeliststxt-checklist)
- [App vs build directory](#app-vs-build-directory)
- [windeployqt](#windeployqt)

---

## Project structure

```
VirtualBench/
├── app/                        # Runtime directory -- exe + Qt DLLs
│   └── sketches/               # User sketches saved here
├── src/
│   ├── main.cpp
│   ├── ui/                     # Qt6 UI layer
│   │   ├── mainwindow.cpp/h    # Main window, layout, toolbar
│   │   ├── canvaswidget.cpp/h  # Circuit canvas (QGraphicsView)
│   │   ├── signaltimeline.cpp/h# Logic analyzer waveform view
│   │   ├── codehighlighter.cpp/h # Syntax highlighting
│   │   └── linenumberarea.h    # Line number gutter
│   └── core/
│       ├── runtime/            # Virtual Arduino runtime
│       │   ├── arduinoapi.h    # ArduinoAPI function pointer struct
│       │   └── arduinoruntime.cpp/h # All impl_* functions + RuntimeState
│       ├── host/               # Sketch DLL loader + hot-reload
│       │   ├── sketchhost.cpp/h     # LoadLibrary, reload logic
│       │   └── sketchhostthread.cpp/h # QThread wrapper
│       ├── build/              # Compiler pipeline
│       │   ├── compiler.cpp/h       # g++ subprocess wrapper
│       │   └── preprocessor.cpp/h   # Arduino → VirtualBench transform
│       └── circuit/            # Auto circuit detection
│           └── circuitdetector.cpp/h
├── sketches/                   # Example sketches (tracked in git)
├── testfiles/                  # Standalone test programs (not Qt)
└── CMakeLists.txt
```

---

## How the simulation works

```
User hits Run
     │
     ▼
Preprocessor transforms Arduino syntax → VirtualBench DLL format
     │
     ▼
Compiler (g++ -shared) builds sketch.dll
     │
     ▼
SketchHost::load()
  - CopyFile(sketch.dll → sketch.dll.tmp.dll)   // Windows file lock workaround
  - LoadLibrary(sketch.dll.tmp.dll)
  - GetProcAddress(vb_init / vb_setup / vb_loop)
  - ArduinoRuntime::get_api() → ArduinoAPI table
  - vb_init(&api)                                // inject function pointers
  - vb_setup()                                   // run Arduino setup()
     │
     ▼
SketchThread loop (background thread):
  - vb_loop() → sketch calls api->digitalWrite() etc.
  - impl_digitalWrite() fires → on_pin_changed callback
  - on_pin_changed emits Qt signal pinChanged(pin, value)
  - Qt queues signal across threads (thread-safe)
  - UI thread receives signal → updates canvas + timeline
```

---

## Adding a new Arduino API function

Example: adding `tone(pin, frequency)`.

### Step 1 — Add to `ArduinoAPI` struct (`src/core/runtime/arduinoapi.h`)

Add the function pointer to the struct. Always add at the END to maintain backwards compatibility with already-compiled sketch DLLs:

```cpp
struct ArduinoAPI {
    // ... existing functions ...

    // Tone -- add at end
    void (*tone)  (int pin, unsigned int frequency);
    void (*noTone)(int pin);
};
```

### Step 2 — Declare the implementation in `arduinoruntime.h`

Add a `static` declaration in the private section of `ArduinoRuntime`. Must be static so its address can be stored in a function pointer:

```cpp
private:
    // ... existing declarations ...
    static void impl_tone  (int pin, unsigned int frequency);
    static void impl_noTone(int pin);
```

### Step 3 — Write the implementation in `arduinoruntime.cpp`

Add the function body. Use `g_runtime` to access `state_`. Emit via callback if set, otherwise fall back to console:

```cpp
void ArduinoRuntime::impl_tone(int pin, unsigned int frequency) {
    if (!g_runtime || pin < 0 || pin >= 20) return;
    // Store frequency in state for later (add tone_frequency to RuntimeState if needed)
    // Notify UI via callback
    if (g_runtime->on_pin_changed)
        g_runtime->on_pin_changed(pin, 1);  // treat as HIGH while tone plays
    else
        std::cout << ts() << "  tone(" << pin << ", " << frequency << ")\n";
}

void ArduinoRuntime::impl_noTone(int pin) {
    if (!g_runtime || pin < 0 || pin >= 20) return;
    if (g_runtime->on_pin_changed)
        g_runtime->on_pin_changed(pin, 0);
    else
        std::cout << ts() << "  noTone(" << pin << ")\n";
}
```

### Step 4 — Wire into `get_api()` in `arduinoruntime.cpp`

```cpp
ArduinoAPI ArduinoRuntime::get_api() {
    g_runtime = this;
    ArduinoAPI api;
    // ... existing assignments ...
    api.tone   = impl_tone;
    api.noTone = impl_noTone;
    return api;
}
```

### Step 5 — Add to preprocessor (`src/core/build/preprocessor.cpp`)

In `replace_api_calls()`, add the string replacement. Order matters — more specific patterns first:

```cpp
s = replace_all(s, "noTone(", "api->noTone(");  // noTone before tone
s = replace_all(s, "tone(",   "api->tone(");
```

Also add to the error message cleanup in `mainwindow.cpp` if the function name appears in error output and needs to be cleaned up.

### Step 6 — Add to syntax highlighter (`src/ui/codehighlighter.cpp`)

In the `arduino_fns` list:

```cpp
const QStringList arduino_fns = {
    // ... existing functions ...
    "tone", "noTone",
};
```

### Step 7 — Test

Write a sketch that calls `tone()` and verify it compiles, runs, and produces the expected output.

---

## Adding a new component

Example: adding a potentiometer.

### Step 1 — Add to `ComponentType` enum (`src/core/circuit/circuitdetector.h`)

```cpp
enum class ComponentType {
    LED,
    Button,
    Buzzer,
    Servo,
    Potentiometer,  // add here
    LCD,
    GenericOutput,
    GenericInput,
    Serial
};
```

### Step 2 — Add detection keywords (`src/core/circuit/circuitdetector.cpp`)

In `infer_type()`, add before the generic fallbacks. More specific keywords first:

```cpp
if (contains_any(upper, {"POT", "POTENTIOMETER", "SENSOR",
                          "ANALOG", "ADC", "PHOTO", "LDR",
                          "TEMP", "LIGHT_SENSOR"}))
    return ComponentType::Potentiometer;
```

### Step 3 — Add label string (`src/core/circuit/circuitdetector.cpp`)

In `detect()`, add to the `type_str` block:

```cpp
case ComponentType::Potentiometer: type_str = "Potentiometer"; break;
```

### Step 4 — Add colors (`src/ui/canvaswidget.cpp`)

In `componentColor()`, add to both maps:

```cpp
static const std::map<ComponentType, QColor> activeColors = {
    // ... existing ...
    { ComponentType::Potentiometer, QColor("#44ffcc") },
};

static const std::map<ComponentType, QColor> inactiveColors = {
    // ... existing ...
    { ComponentType::Potentiometer, QColor("#1a1a3a") },
};
```

### Step 5 — Add visual rendering (`src/ui/canvaswidget.cpp`)

In `drawComponent()`, add any special rendering. For a potentiometer this could be a value slider drawn inside the box. For a basic version just let it render as a labeled box like other components — the color distinguishes it.

If the component goes on the left side (input) or right side (output), update the `is_output` check:

```cpp
bool is_output = (comp.type == ComponentType::LED      ||
                  comp.type == ComponentType::Buzzer   ||
                  comp.type == ComponentType::Servo    ||
                  comp.type == ComponentType::GenericOutput);
// Potentiometer is input so it goes on the left -- no change needed
```

### Step 6 — Add runtime interaction if needed

If the component injects values into the simulation (potentiometer → `analogRead`):

**Add to `ArduinoRuntime`** (`arduinoruntime.h`):
```cpp
void inject_analog(int pin, int value) {
    if (pin >= 0 && pin < 8)
        state_.analog_values[pin] = value;
}
```

**Add to `SketchHost`** (`sketchhost.h`):
```cpp
void inject_analog(int pin, int value) {
    runtime_.inject_analog(pin, value);
}
```

**Add to `SketchThread`** (`sketchhostthread.h/.cpp`):
```cpp
// .h
void injectAnalog(int pin, int value);

// .cpp
void SketchThread::injectAnalog(int pin, int value) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_analog(pin, value);
}
```

**Add signal to `CanvasWidget`** (`canvaswidget.h`):
```cpp
signals:
    void buttonPressed(int pin, int value);
    void potentiometerChanged(int pin, int value);  // new
```

**Connect in `MainWindow::buildCanvasPanel()`** (`mainwindow.cpp`):
```cpp
connect(canvasWidget_, &CanvasWidget::potentiometerChanged,
        this, [this](int pin, int value) {
            sketchThread_->injectAnalog(pin, value);
        });
```

### Step 7 — Test

Write a sketch that reads the new component and verify detection, canvas rendering, and runtime injection all work.

---

## Adding a new Arduino board

Currently hardcoded for Uno (ATmega328P, 14 digital pins, 6 analog pins):

1. **`RuntimeState`** in `arduinoruntime.h` — increase array sizes if board has more pins
2. **`pinLocation()`** in `canvaswidget.cpp` — add a new layout for the board's pin arrangement
3. **`drawBoard()`** in `canvaswidget.cpp` — add a new board graphic
4. **Toolbar board label** in `mainwindow.cpp` — make it a dropdown selector
5. **Compiler flags** in `compiler.cpp` — may need different `-mmcu` flag if using avr-gcc

---

## Thread safety rules

Two threads always run simultaneously:
- **UI thread** — Qt event loop, all widget updates
- **SketchThread** — simulation loop, calls `vb_loop()` continuously

Rules:
- Never access Qt widgets from `SketchThread` — always use signals/slots
- Never call `host_` methods from the UI thread without `inject_mutex_`
- Qt signal/slot connections across threads are automatically queued — don't add extra mutexes around them
- The `on_pin_changed` and `on_serial_output` callbacks fire on `SketchThread` — they emit Qt signals which Qt safely delivers to the UI thread

---

## Hot-reload gotchas

- Sketch state (variables, counters) resets on every reload because `vb_setup()` is called fresh
- If a sketch has an infinite loop with no `delay()`, `SketchThread` never yields and the UI appears frozen — always include at least a small `delay()` in `loop()`
- The `.tmp.dll` copy is essential on Windows — Windows locks DLLs while loaded so the compiler can't overwrite `sketch.dll` if it's loaded directly. Always load the copy.
- After `FreeLibrary`, all function pointers from that DLL are invalid — never call them after unloading

---

## Preprocessor line number offset

The preprocessor injects a header at the top of every transformed sketch. This shifts all line numbers in compiler error messages. The offset is tracked in:

```cpp
// preprocessor.h
static constexpr int INJECTED_HEADER_LINES = 10;
```

**If you change the injected header in `preprocessor.cpp`, recount the lines and update this constant.** Count every `\n` in the header string in `inject_header()`. If this gets out of sync, error highlighting in the editor will point to the wrong lines.

---

## Circuit detection ordering

In `circuitdetector.cpp`, `infer_type()` returns on the first keyword match. Rules:

- Put more specific keywords before general ones
- `LED` before `GenericOutput`, `BUTTON` before `GenericInput`
- New component keywords go before the generic fallbacks at the bottom
- The fallback is always mode-based: `OUTPUT` → `GenericOutput`, anything else → `GenericInput`

---

## CMakeLists.txt checklist

Every time you add a new `.cpp` file:
1. Add it to the `SOURCES` list in `CMakeLists.txt`
2. Add the header to `HEADERS` (optional but good for IDE)
3. Rerun cmake configure — `cmake --build` alone is not enough after changing `CMakeLists.txt`

```powershell
cmake -B build -S . -G "Ninja" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe"
cmake --build build
```

---

## App vs build directory

- **`app/`** — runtime directory. `QCoreApplication::applicationDirPath()` points here. Sketches save to `app/sketches/`. Never put source files here.
- **`build/`** — CMake's working directory. Never manually edit anything here. Safe to delete entirely and regenerate with the cmake configure command above.

---

## windeployqt

Copies all required Qt DLLs and plugins next to the exe. Only needs to be run:
- Once after the first build
- After a Qt version upgrade
- After moving the exe to a new location

```powershell
C:\Qt\6.11.1\mingw_64\bin\windeployqt.exe .\app\VirtualBench.exe
```

If the app launches and immediately closes with no error, missing DLLs are almost always the cause — rerun windeployqt.

---

## Things that will trip you up
The INJECTED_HEADER_LINES constant is the most likely source of subtle bugs — if you ever modify the injected header and forget to recount the lines, error highlighting silently breaks. Make it a habit to recount whenever you touch inject_header().

When adding analog injection for the potentiometer, remember that analogRead returns 0-1023 on a real Arduino (10-bit ADC), not 0-255. Your slider UI should reflect that range.
If you ever add a component that needs to run code on a timer (like a servo that animates smoothly), don't put a loop in the UI thread — use a QTimer instead. Blocking the UI thread even for milliseconds makes the whole app feel sluggish.

### The next hardest thing you'll hit
The variable watch panel is trickier than it looks. The simple approach is adding a watch_variable(name, value) function to ArduinoAPI and having sketches call it explicitly. The hard approach is reading DWARF debug info from the compiled DLL to watch variables automatically without modifying the sketch. Start with the simple approach.

### Before you ship to anyone
The compiler and include paths are still hardcoded to your machine in mainwindow.cpp. Before anyone else can use VirtualBench you'll need a first-run settings dialog that detects or asks for the compiler path. That's the single biggest blocker for distribution.
