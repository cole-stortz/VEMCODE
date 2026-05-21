# VirtualBench

An open-source embedded systems simulator for writing, simulating, testing, and debugging Arduino code — no hardware required.

![Status](https://img.shields.io/badge/status-active%20development-orange)
![Platform](https://img.shields.io/badge/platform-Windows-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## What it does

Write standard Arduino sketches directly in the built-in editor and simulate them instantly — no Arduino board, no USB cable, no waiting. VirtualBench compiles your sketch to a native DLL and runs it against a virtual Arduino runtime in real time.

- **Write** Arduino code in a syntax-highlighted editor with auto-indent, line numbers, and compile error highlighting
- **Simulate** instantly — hit Run and your sketch compiles and executes in milliseconds
- **Visualize** — the circuit canvas auto-detects components from your code and renders them automatically
- **Interact** — click buttons, toggle switches, drag potentiometers, and type serial input to interact with the running simulation
- **Debug** — serial monitor, signal timeline (logic analyzer view), and variable watch panel
- **Hot-reload** — edit your sketch, hit Run again, simulation restarts instantly
- **Speed control** — slow down or speed up simulation from 0.1x to 2.5x

## Screenshots

*Coming soon*

---

## Features

### Editor
- Syntax highlighting (keywords, functions, strings, comments)
- Line numbers
- Auto-indent — Enter after `{` indents automatically
- Auto-dedent — typing `}` on an indented blank line dedents automatically
- Tab = 4 spaces
- Compile error highlighting — error lines turn red with tooltip showing the message
- Ctrl+S to save
- New sketch, Open, Save, Recent sketches (last 5)

### Simulation
- Full Arduino API support:
  - `pinMode`, `digitalWrite`, `digitalRead`
  - `analogWrite`, `analogRead`
  - `delay`, `delayMicroseconds`, `millis`, `micros`
  - `pulseIn(pin, value, timeout)` — fast path for sensors, color channel routing for TCS3200, pin polling fallback
  - `Serial.begin`, `Serial.print`, `Serial.println`, `Serial.available`, `Serial.read`
  - `tone`, `noTone`
  - `map`, `constrain`, `abs`, `min`, `max`, `random`
- Full `String` class — construction, concatenation, search, manipulation, conversion
- Speed slider — 0.1x to 2.5x simulation speed
- Stop is instant — no waiting for long delays to finish

### Circuit Canvas
Auto-detects components from `#define` names and `pinMode` / `analogRead` calls:

| Component | Detection keywords | Interaction |
|---|---|---|
| LED | LED, LIGHT, LAMP, INDICATOR | Visual on/off |
| Button | BTN, BUTTON, KEY | Click to press |
| Switch | SWITCH, SW, TOGGLE | Click to toggle |
| Buzzer | BUZZER, BUZZ, SPEAKER, TONE, PIEZO | Visual active state |
| Servo | SERVO, SRV | Live angle display (0-180°) |
| H-bridge motor | MOTOR, CW, CWISE, ANTI, IN1, IN2 | Visual active state |
| Distance sensor | TRIG, ECHO (pair) | Type distance in cm — pulseIn returns matching µs |
| Color sensor | S0, S1, S2, S3, COLOR_OUT (array) | Type R/G/B values (0-255) |
| Potentiometer | POT, POTENTIOMETER, DIAL | Drag to set value 0-1023 |
| Light sensor | PHOTO, LDR, PHOTORESISTOR | Type analog value (0-1023) |
| Temperature sensor | TEMP, TEMPERATURE, THERMISTOR | Type analog value (0-1023) |
| Analog sensor | SENSOR, ANALOG, ADC | Type analog value (0-1023) |
| LCD | LCD, DISPLAY, SCREEN, OLED | Visual coming soon |

### Debug Panel
- **Serial monitor** — live `Serial.print` output, plus a text input box to send data to `Serial.read`
- **Signal timeline** — logic analyzer view of all pin state changes over time
- **Variable watch** — live table of `watch_variable("name", value)` calls from your sketch

---

## Getting started

### Prerequisites

- Windows 10/11 64-bit
- Qt 6.x with MinGW 64-bit — [download from qt.io](https://www.qt.io/download)
  - During install select: Qt 6.x → MinGW 64-bit, and Developer Tools → Ninja
- CMake 3.20+

### Build from source

```powershell
# Clone
git clone https://github.com/cole-stortz/VirtualBench.git
cd VirtualBench

# Configure (all one line)
cmake -B build -S . -G "Ninja" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe"

# Build
cmake --build build

# Deploy Qt runtime (first time only)
C:\Qt\6.11.1\mingw_64\bin\windeployqt.exe app\VirtualBench.exe
```

> **Note:** Ninja may be at a different path depending on your system. Run `where.exe ninja` to find it.

### First run

```powershell
.\app\VirtualBench.exe
```

On first launch VirtualBench will ask for your compiler path and project root. Point it at your `g++.exe` (e.g. `C:/Qt/Tools/mingw1310_64/bin/g++.exe`) and the root of the VirtualBench repo. These are saved to `app/settings.ini`.

---

## Writing sketches

VirtualBench accepts standard Arduino syntax — write exactly what you would write for a real Arduino:

```cpp
#define LED_PIN    13
#define BUTTON_PIN  2

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("Ready");
}

void loop() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Button pressed");
    } else {
        digitalWrite(LED_PIN, LOW);
    }
    delay(50);
}
```

The preprocessor automatically transforms your sketch into the VirtualBench runtime format. You never write any boilerplate.

### String support

The full Arduino `String` class is available:

```cpp
String msg = String("Count: ") + String(counter);
Serial.println(msg);
```

### Variable watch

Use `watch_variable` to monitor values in real time in the Variable Watch panel:

```cpp
watch_variable("counter", counter);
watch_variable("sensor", analogRead(A0));
```

### Serial input

Type into the serial monitor input box and hit Send (or Enter) to inject data into `Serial.available()` / `Serial.read()`:

```cpp
if (Serial.available() > 0) {
    int c = Serial.read();
    Serial.println(c);
}
```

### Non-blocking patterns

`millis()` works correctly for non-blocking timing:

```cpp
unsigned long last = 0;

void loop() {
    if (millis() - last >= 500) {
        last = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
}
```

---

## Architecture

VirtualBench compiles your sketch into a `.dll` using the system C++ compiler and loads it at runtime via `LoadLibrary`. The sketch calls back into the host through a function pointer table (`ArduinoAPI`) — so `digitalWrite(13, HIGH)` in your sketch calls `impl_digitalWrite` in the host, which updates the canvas and signal timeline in real time.

```
Your sketch (.cpp)
    → Preprocessor (transforms Arduino syntax → DLL format)
    → g++ (compiles to .dll)
    → SketchHost (LoadLibrary, extracts vb_init/vb_setup/vb_loop)
    → SketchThread (runs vb_loop in background thread)
    → ArduinoRuntime (implements all API calls, fires callbacks)
    → UI (canvas, serial monitor, signal timeline, variable watch)
```

Hot-reload works by watching the sketch file for changes and reloading the DLL while the simulation is running.

### Project structure

```
VirtualBench/
├── app/                        # Runtime — exe + Qt DLLs
│   ├── sketches/               # Saved sketches
│   └── settings.ini            # Compiler path + recent sketches (gitignored)
├── src/
│   ├── main.cpp
│   ├── ui/
│   │   ├── mainwindow.cpp/h    # Main window, toolbar, all UI wiring
│   │   ├── canvaswidget.cpp/h  # Circuit canvas + component rendering
│   │   ├── signaltimeline.cpp/h
│   │   ├── codehighlighter.cpp/h
│   │   ├── linenumberarea.cpp/h
│   │   ├── variablewatch.cpp/h
│   │   └── settingsdialog.cpp/h
│   └── core/
│       ├── runtime/
│       │   ├── arduinoapi.h        # ArduinoAPI function pointer struct
│       │   └── arduinoruntime.cpp/h
│       ├── host/
│       │   ├── sketchhost.cpp/h        # DLL load/unload + hot-reload
│       │   └── sketchhostthread.cpp/h  # Background simulation thread
│       ├── build/
│       │   ├── compiler.cpp/h      # Invokes g++
│       │   └── preprocessor.cpp/h  # Arduino → VirtualBench transform
│       └── circuit/
│           └── circuitdetector.cpp/h   # Auto component detection
├── sketches/                   # Example sketches
└── CMakeLists.txt
```

---

## Roadmap

### Phase 1 — Component Completion (mostly complete)

**Completed:**
- ✓ `pulseIn(pin, value, timeout)` — distance sensor fast path, TCS3200 color channel path, pin polling fallback
- ✓ `delayMicroseconds` — busy-wait with stop check
- ✓ `analogWrite` fires pin changed callback for signal timeline
- ✓ Array-based pin detection (`const int PIN[N] = {...}`)
- ✓ Multi-pin component grouping (HC-SR04 → DistanceSensor, H-bridge → HBridgeMotor, TCS3200 → ColorSensor)
- ✓ Motor (H-bridge) separated from Servo (PWM single pin)
- ✓ Canvas sensor inputs — distance (cm), color (R/G/B 0-255), analog (0-1023)
- ✓ Servo angle display — live °label from analogWrite value

**Remaining:**
- Servo class in injected header + `#include <Servo.h>` stripping

> **Milestone:** The simplified Lambo robot sketch (1 color sensor, 1 HC-SR04, 3 H-bridge motors, 1 servo) compiles and runs correctly.

### Phase 2 — Component Visuals
- Proper graphics for all component types
- Per-component sensor input boxes (type a distance value, watch your code react)

### Phase 3 — Canvas Improvements
- Canvas layout mode — drag components to match your real breadboard, positions saved per sketch
- Wire visualization improvements

### Phase 4 — Simulation Realism
- Floating pin simulation (undriven INPUT pins return random values)
- Button bounce simulation
- Optional signal noise on analog readings

### Phase 5 — New Arduino Features
- `attachInterrupt()` — RISING, FALLING, CHANGE modes
- EEPROM simulation (1024 bytes, optional disk persistence)
- Basic I2C/SPI simulation

### Phase 6 — Display Support
- 16x2 LCD (`LiquidCrystal` compatible)
- 7-segment display
- Basic OLED

### Phase 7 — Multi-board Simulation
- Two Arduinos communicating over virtual serial

### Phase 8 — Memory Analysis
- Dual compile with `avr-gcc` for flash and RAM size analysis
- Flash → hard enforce (block run if over 32,256 bytes)
- Static RAM → hard enforce (block run if globals exceed 2048 bytes)
- Dynamic RAM (String/malloc) → warn but don't block
- Memory bar in UI like Arduino IDE

### Later
- macOS / Linux support
- Installer (bundle MinGW, zero-dependency install)
- Additional board support (Nano, Mega, ESP32)

### Known limitations
- Microsecond-accurate timing (Windows thread scheduler has ~1-15ms jitter)
- Hardware protocol electrical behavior (I2C/SPI bus characteristics)
- Register-level / AVR assembly programming
- Dynamic RAM enforcement is approximate — static RAM hard enforced via avr-gcc, heap usage tracked with warnings
- Libraries that wrap AVR hardware registers directly

---

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Pull requests welcome. If you find a bug or want a feature, open an issue.