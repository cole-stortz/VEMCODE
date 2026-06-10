# Adding Components and Arduino API Functions

This guide covers every file you need to touch when adding a new simulated component or a new Arduino API function to VEMCODE. Read ARCHITECTURE.md first if you haven't already.

---

## Vocabulary

| Term | Meaning |
|---|---|
| **Component** | A visual element on the canvas (LED, button, sensor, etc.) that maps to one or more pins |
| **Arduino API function** | A function a sketch calls (`digitalWrite`, `analogRead`, etc.) — lives in `ArduinoAPI` struct |
| **Detection** | How `CircuitDetector` decides a component exists by reading sketch source |
| **Rendering** | How `CanvasWidget` draws the component and handles mouse input |
| **Injection** | Pushing a value from the UI into the running sketch (e.g. setting a sensor reading) |

---

## Part 1 — Adding a New Component

Adding a component has four parts: detection, rendering, interaction, and (if needed) a new API function or inject path.

### Step 1 — Add the enum value

File: [src/core/circuit/circuitdetector.h](../src/core/circuit/circuitdetector.h)

Add your type to `ComponentType`:

```cpp
enum class ComponentType {
    LED, Button, Switch, Buzzer, Servo,
    Potentiometer, LightSensor, TempSensor, AnalogSensor,
    LCD, GenericOutput, GenericInput, Serial,
    DistanceSensor, HBridgeMotor, ColorSensor,
    YourNewComponent   // <-- add here
};
```

### Step 2 — Teach CircuitDetector to recognize it

File: [src/core/circuit/circuitdetector.cpp](../src/core/circuit/circuitdetector.cpp)

There are two paths depending on whether your component uses one pin or multiple pins.

#### Single-pin component (e.g. LED, Button, Buzzer)

Add keywords to `infer_type()` (around line 435). Keywords are matched case-insensitively against the `#define` name:

```cpp
if (contains_any(upper, {"YOURKEY", "ANOTHER_KEY"}))
    return ComponentType::YourNewComponent;
```

Add the human-readable label to the `type_labels` map at the top of `detect()`:

```cpp
{ ComponentType::YourNewComponent, "Your Component" },
```

That's it — phase 1 (`parse_pinmodes`) and phase 2 (`analogRead` scan) will pick it up automatically.

#### Multi-pin component (e.g. DistanceSensor, HBridgeMotor, ColorSensor)

Multi-pin components must be handled in `detect_multipin()` so their pins are **claimed** before phases 1 and 2 run. Add detection logic inside `detect_multipin()` above the `return claimed;` line:

```cpp
// --- YourSensor ---
// Collect defines that match your naming pattern
int pin_a = -1, pin_b = -1;
for (const auto& d : defines) {
    std::string upper = to_upper(d.first);
    int pin = resolve_pin(d.second, defines);
    if (pin < 0) continue;
    if (contains_any(upper, {"YOURA"})) pin_a = pin;
    if (contains_any(upper, {"YOURB"})) pin_b = pin;
}
if (pin_a >= 0 && pin_b >= 0) {
    DetectedComponent comp;
    comp.type     = ComponentType::YourNewComponent;
    comp.pin      = pin_a;                  // primary/representative pin
    comp.pins     = {pin_a, pin_b};         // all pins
    comp.pin_name = "YourSensor";
    comp.label    = "Your Sensor (A=" + std::to_string(pin_a)
                  + ", B=" + std::to_string(pin_b) + ")";
    comp.confirmed = false;
    components_.push_back(comp);
    claimed.insert(pin_a);
    claimed.insert(pin_b);
}
```

The `claimed` set prevents phases 1 and 2 from creating duplicate single-pin entries for the same pins.

#### Detection keyword reference (existing)

```
LED:           LED, LIGHT, LAMP, INDICATOR
Button:        BTN, BUTTON, KEY
Switch:        SWITCH, SW, TOGGLE
Buzzer:        BUZZER, BUZZ, SPEAKER, TONE, PIEZO
Servo:         SERVO, SRV
Potentiometer: POT, POTENTIOMETER, DIAL
LightSensor:   PHOTO, LDR, LIGHT_SENSOR, PHOTORESISTOR
TempSensor:    TEMP, TEMPERATURE, THERMISTOR
AnalogSensor:  SENSOR, ANALOG, ADC
LCD:           LCD, DISPLAY, SCREEN, OLED
DistanceSensor: TRIG + ECHO pair (multi-pin phase 0)
HBridgeMotor:  MOTOR, CW, CWISE, ANTI, IN1-IN4 group (multi-pin phase 0)
ColorSensor:   S0/S1/S2/S3/OUT arrays (multi-pin phase 0)
```

### Step 3 — Render the component on the canvas

File: [src/ui/canvaswidget.cpp](../src/ui/canvaswidget.cpp)

Find `drawComponent()` and add a case for your type. Look at how existing components are drawn for sizing conventions. The coordinate system:

- **Output components** (driven by the sketch): drawn on the right side, Y aligned to the pin row
- **Digital inputs** (user interacts with them): drawn in the left inner column (`BOARD_X - comp_w - 80`)
- **Analog sensors** (injected values): drawn in the left outer column (`BOARD_X - comp_w - 180`)

Minimal skeleton:

```cpp
case ComponentType::YourNewComponent: {
    // Draw your component using QPainter p
    p.setBrush(Qt::darkCyan);
    p.drawRect(x, y, comp_w, comp_h);
    p.drawText(x, y + comp_h + 14, QString::fromStdString(comp.label));
    break;
}
```

#### If your component needs a canvas input (QLineEdit)

Follow the DistanceSensor / ColorSensor pattern. Create a `QGraphicsProxyWidget` wrapping a `QLineEdit` inside `drawComponent()`, connect its `textChanged` signal, then call `setText(defaultValue)` **after** connecting the signal so the initial injection fires:

```cpp
auto* input = new QLineEdit();
auto* proxy = scene()->addWidget(input);
proxy->setPos(x + comp_w + 5, y);
// connect FIRST, then setText — textChanged doesn't fire from constructor
connect(input, &QLineEdit::textChanged, this, [this, comp](const QString& text) {
    bool ok;
    int val = text.toInt(&ok);
    if (ok) emit analogInjected(comp.pin, val);
});
input->setText("512");  // triggers initial injection
```

### Step 4 — Wire up mouse interaction (input components only)

File: [src/ui/canvaswidget.cpp](../src/ui/canvaswidget.cpp)

Add cases to `mousePressEvent` / `mouseReleaseEvent` / `mouseMoveEvent` as needed. Emit existing signals (`buttonPressed`, `potentiometerChanged`) or add a new signal if your interaction type is unique. Signals go to `MainWindow`, which calls the appropriate inject method on `SketchThread`.

---

## Part 2 — Adding a New Arduino API Function

Adding an API function touches six files in a specific order.

### Step 1 — Declare the function pointer in ArduinoAPI

File: [src/core/runtime/arduinoapi.h](../src/core/runtime/arduinoapi.h)

Add the new function pointer **at the end** of the struct. Adding in the middle breaks binary compatibility with already-compiled sketch DLLs:

```cpp
struct ArduinoAPI {
    // ... existing fields ...

    // NEW — always append at the end
    void (*yourFunction)(int param);
};
```

### Step 2 — Declare the impl_ method

File: [src/core/runtime/arduinoruntime.h](../src/core/runtime/arduinoruntime.h)

Add a static method declaration in the `private:` section:

```cpp
static void impl_yourFunction(int param);
```

### Step 3 — Implement the function

File: [src/core/runtime/arduinoruntime.cpp](../src/core/runtime/arduinoruntime.cpp)

Add the implementation. All `impl_*` functions are static and access runtime state through the global `g_runtime` pointer:

```cpp
void ArduinoRuntime::impl_yourFunction(int param) {
    if (!g_runtime) return;
    // read/write g_runtime->state_ here
    // fire callbacks like:
    if (g_runtime->on_pin_changed)
        g_runtime->on_pin_changed(param, someValue);
}
```

### Step 4 — Wire it into get_api()

File: [src/core/runtime/arduinoruntime.cpp](../src/core/runtime/arduinoruntime.cpp)

In `get_api()`, assign your impl to the struct field:

```cpp
ArduinoAPI ArduinoRuntime::get_api() {
    g_runtime = this;
    ArduinoAPI api = {};
    // ... existing assignments ...
    api.yourFunction = impl_yourFunction;
    return api;
}
```

### Step 5 — Add the preprocessor replacement

File: [src/core/build/preprocessor.cpp](../src/core/build/preprocessor.cpp)

In `replace_api_calls()`, add a string replacement so `yourFunction(` in sketches becomes `api->yourFunction(`:

```cpp
source = replace_all(source, "yourFunction(", "api->yourFunction(");
```

**Order matters** — if your function name is a prefix of another, add the longer one first. See the existing ordering notes in ARCHITECTURE.md.

### Step 6 — Expose it in the injected header

File: [src/core/build/injected_header.inc](../src/core/build/injected_header.inc)

Add either a thin inline wrapper (if you need overloads or a default arg) or just a `using` declaration. For a plain passthrough, nothing is strictly required since the preprocessor rewrite handles it — but you may want to add a `using` or inline wrapper for convenience:

```cpp
// Optional: inline wrapper with a default argument
inline void yourFunction(int param, int extra = 0) {
    api->yourFunction(param);
}
```

After editing this file, you **must re-run cmake** (`cmake -B build -S .`) before building. The file is embedded as a C string at configure time. The build system detects the change automatically if you build via `cmake --build build`.

---

## Part 3 — Adding an Inject Path (UI → Running Sketch)

If your component lets the user feed a value into the sketch (like DistanceSensor's cm field), you need an inject path through four layers:

```
CanvasWidget signal
    → MainWindow slot
        → SketchThread::injectXxx()
            → SketchHost::inject_xxx()
                → ArduinoRuntime::inject_xxx()
```

### ArduinoRuntime

Add an `inject_xxx` method to `arduinoruntime.h` and implement it inline or in `arduinoruntime.cpp`. Write directly into `state_`:

```cpp
void inject_yourValue(int pin, int value) {
    if (pin >= 0 && pin < profile_.pin_count)
        state_.your_values_[pin] = value;
}
```

### SketchHost

File: [src/core/host/sketchhost.h](../src/core/host/sketchhost.h)

Add an inline delegator:

```cpp
void inject_your_value(int pin, int value) {
    runtime_.inject_yourValue(pin, value);
}
```

### SketchThread

File: [src/core/host/sketchhostthread.h](../src/core/host/sketchhostthread.h) and [sketchhostthread.cpp](../src/core/host/sketchhostthread.cpp)

Declare the slot and implement it with a mutex lock:

```cpp
// .h
public slots:
    void injectYourValue(int pin, int value);

// .cpp
void SketchHostThread::injectYourValue(int pin, int value) {
    QMutexLocker lock(&inject_mutex_);
    host_.inject_your_value(pin, value);
}
```

### CanvasWidget signal + MainWindow connection

Declare the signal in `canvaswidget.h`:

```cpp
signals:
    void yourValueChanged(int pin, int value);
```

Emit it from the canvas input handler, then connect it in `mainwindow.cpp` where the other canvas signals are wired up:

```cpp
connect(canvasWidget_, &CanvasWidget::yourValueChanged,
        sketchThread_,  &SketchHostThread::injectYourValue);
```

---

---

## Part 4 — Adding a New Board Profile

Adding a new board is a single line in one file. The rest of the system picks it up automatically.

### Step 1 — Add the profile constant

File: [src/core/runtime/boardprofile.h](../src/core/runtime/boardprofile.h)

Add a new `static const BoardProfile` entry:

```cpp
static const BoardProfile BOARD_YOURBOARD = {"Your Board Name", "ChipID", pin_count, analog_offset, analog_count, pwm_resolution};
```

#### Field reference

| Field | What it means | Example values |
|---|---|---|
| `name` | Display name — must match `// @board` comments exactly | `"Arduino Uno"`, `"Teensy 4.1"` |
| `chip` | Chip identifier — display only, not used in logic | `"ATmega328P"`, `"IMXRT1062"` |
| `pin_count` | Total number of pins (digital + analog combined) | Uno=20, Mega=70, Teensy=42 |
| `analog_offset` | Pin number where A0 starts | Uno=14, Mega=54, Teensy=14 |
| `analog_count` | Number of analog pins | Uno=6, Mega=16, Teensy=18 |
| `pwm_resolution` | Max `analogWrite` value | 8-bit boards=255, 12-bit boards=4095 |

#### Existing profiles for reference

```cpp
static const BoardProfile BOARD_UNO    = {"Arduino Uno",       "ATmega328P",   20, 14,  6,  255};
static const BoardProfile BOARD_NANO   = {"Arduino Nano",      "ATmega328P",   22, 14,  8,  255};
static const BoardProfile BOARD_MEGA   = {"Arduino Mega 2560", "ATmega2560",   70, 54, 16,  255};
static const BoardProfile BOARD_DUE    = {"Arduino Due",       "AT91SAM3X8E",  66, 54, 12, 4095};
static const BoardProfile BOARD_TEENSY = {"Teensy 4.1",        "IMXRT1062",    42, 14, 18, 4095};
```

### Step 2 — Register it in the Settings dialog

File: [src/ui/settingsdialog.cpp](../src/ui/settingsdialog.cpp)

Find where the board selector `QComboBox` is populated and add your board name to the list. The name string must be identical to the `name` field in your profile.

### Step 3 — Add a resolution entry in MainWindow

File: [src/ui/mainwindow.cpp](../src/ui/mainwindow.cpp)

Find the function that resolves a board name string to a `BoardProfile` (used by the Settings dialog and `// @board` hint). Add your board:

```cpp
if (name == "Your Board Name") return BOARD_YOURBOARD;
```

That's all. Every system that consumes the profile — `RuntimeState`, `CanvasWidget`, `CircuitDetector`, `inject_analog`, `impl_analogRead`, the servo angle calculation — reads the profile at runtime, so no other files need changes.

### Board profile gotchas

- **`analog_offset` must be exact.** Both `inject_analog` and `impl_analogRead` subtract this from the raw pin number to get the array index. If it's wrong, `analogRead(A0)` returns the value for a different pin.
- **`pwm_resolution` affects servo angle display.** The canvas computes `angle = value * 180 / pwm_resolution`. Use 255 for 8-bit boards, 4095 for 12-bit boards.
- **`pin_count` bounds all pin arrays.** `RuntimeState` arrays are sized 80 — make sure `pin_count` doesn't exceed that, and that `analog_offset + analog_count <= pin_count`.
- **`name` must match `// @board` exactly.** An unrecognized board hint silently falls back to the settings board with no error shown. If your board name has different capitalization or spacing than what users will type, they'll get silent fallback behavior.

---

## Quick Checklist

### New component (no new API function)

- [ ] `ComponentType` enum — `circuitdetector.h`
- [ ] `type_labels` map entry — `circuitdetector.cpp` `detect()`
- [ ] Detection keywords in `infer_type()` OR multi-pin logic in `detect_multipin()` — `circuitdetector.cpp`
- [ ] `drawComponent()` case — `canvaswidget.cpp`
- [ ] Mouse interaction (if input component) — `canvaswidget.cpp`
- [ ] Inject path (if sensor) — runtime → host → thread → mainwindow

### New Arduino API function

- [ ] Function pointer at END of `ArduinoAPI` — `arduinoapi.h`
- [ ] `impl_*` static declaration — `arduinoruntime.h`
- [ ] `impl_*` implementation — `arduinoruntime.cpp`
- [ ] Assign in `get_api()` — `arduinoruntime.cpp`
- [ ] Preprocessor string replacement — `preprocessor.cpp`
- [ ] Sketch-facing declaration or wrapper — `injected_header.inc`
- [ ] Re-run cmake after editing `injected_header.inc`

### New board profile

- [ ] `static const BoardProfile` entry — `boardprofile.h`
- [ ] Board name added to selector — `settingsdialog.cpp`
- [ ] Name-to-profile resolver entry — `mainwindow.cpp`

---

## Gotchas

- **Multi-pin detection must run before single-pin detection.** Always add multi-pin sensors to `detect_multipin()` and insert pins into `claimed`. If you skip this, phases 1 and 2 will create duplicate single-pin entries for those pins.
- **Always append to ArduinoAPI, never insert in the middle.** The struct layout is the ABI between the host and sketch DLLs. Inserting a field shifts all subsequent offsets and breaks any previously compiled `.dll`/`.so`.
- **`injected_header.inc` changes require a cmake re-run.** `cmake --build build` detects it automatically, but `g++` invoked directly will not.
- **Connect canvas input signals before calling `setText()`.** `QLineEdit::textChanged` does not fire during construction, so the initial injection only happens if `setText` is called after the signal is connected.
- **Callbacks fire on the sketch background thread.** Inside `impl_*` functions, only call `on_pin_changed`, `on_serial_output`, or `on_variable_changed`. Never touch Qt widgets directly from an `impl_*` function.
- **`analog_index` offset.** `inject_analog` and `impl_analogRead` both apply `pin >= analog_offset ? pin - analog_offset : pin`. New analog-based sensors must use `inject_analog`, not `inject_pin`, so this offset is applied correctly.
- **Preprocessor replace order.** If your new function name is a prefix of an existing one (or vice versa), add the longer name's replacement first to avoid partial substitution.
