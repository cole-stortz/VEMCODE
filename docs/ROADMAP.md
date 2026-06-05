# VEMCODE — Roadmap

Completed phases are marked ✓. Active and future phases are in order of planned work.

---

### Phase 0 — Core Infrastructure ✓

- ✓ Initial file structure and CMakeLists.txt
- ✓ `ArduinoAPI` function pointer table — all Arduino calls go through injected struct
- ✓ `ArduinoRuntime` — implements all `impl_*` functions, owns simulation state
- ✓ `SketchThread` — QThread running `vb_loop()` on a background thread
- ✓ First working Qt6 GUI with serial monitor output
- ✓ AutoCompile pipeline — file watch → g++ invocation → DLL hot-reload
- ✓ Output DLL placed in sketch subfolder (not build dir)
- ✓ Initial Preprocessor — transforms Arduino source to DLL format (`vb_init`, `vb_setup`, `vb_loop`)
- ✓ Circuit canvas (`CanvasWidget`) with basic component rendering
- ✓ Clickable Button component on canvas
- ✓ `CircuitDetector` keyword scan — detects component types from `#define` names
- ✓ Delay consistency — simulated delay tracks sketch timing

> **Milestone:** "Blink" sketch compiles, loads, and toggles the LED on the canvas. ✓

---

### Phase 1 — Editor and Language Features ✓

- ✓ Syntax highlighting — blue keywords, yellow functions, green comments, orange strings
- ✓ Compile error highlighting — red line backgrounds via `QTextEdit::ExtraSelection`
- ✓ Error line number correction — subtracts `INJECTED_HEADER_LINES` so errors point to user sketch lines
- ✓ Corrected error message names (temp file path stripped, `api->` prefix stripped)
- ✓ Line number gutter — `EditorWithLines` + `LineNumberArea` subclass
- ✓ Auto-indent and auto-dedent — Enter carries indentation, Tab inserts 4 spaces, `}` dedents
- ✓ Variable watch panel — `QTableWidget` updated from `watch_variable()` callbacks
- ✓ `Serial.println` type overloads — int, float, String, const char*
- ✓ `String` class — wraps `std::string`, injected into preprocessor header
- ✓ Math functions — `map()`, `constrain()`, `abs()`, `min()`, `max()`, `random()`
- ✓ Safety delay injection — preprocessor inserts `api->delay(10)` if no delay found in `loop()`, prevents infinite loop crash

> **Milestone:** Non-trivial sketches using String and math helpers compile and run without crashes. ✓

---

### Phase 2 — UI Polish and User Workflow ✓

- ✓ First-run settings dialog — compiler path and project root saved to `app/settings.ini`
- ✓ New Sketch button — creates empty sketch in a new subfolder
- ✓ Recent Sketches button — last 5 paths persisted in `settings.ini`
- ✓ Speed slider — range 1–25 (= 0.1x–2.5x), passed to runtime as `speed_multiplier = 1/speed`
- ✓ Stop delay fix — `impl_delay` sleeps in 10ms chunks, checks `stop_requested_` between each chunk
- ✓ `Serial.available()` / `Serial.read()` — UI text input feeds `serial_buffer_`, consumed by runtime
- ✓ Switch component — toggles state on click, persists in `switchStates_` QMap
- ✓ Potentiometer component — drag up/down changes analog value 0–1023
- ✓ Two-column canvas layout — inputs left, outputs right, pin-aligned wiring
- ✓ Signal timeline — logic analyzer waveform view for digital pin state history

> **Milestone:** Full interactive sketch workflow: open, edit, compile, run, adjust inputs, stop. ✓

---

### Phase 3 — Component Completion ✓

- ✓ `pulseIn(pin, value, timeout)` — fast path (distance sensor), color channel path (TCS3200), slow path (pin polling)
- ✓ `delayMicroseconds` — busy-wait with stop check
- ✓ `analogWrite` fires `on_pin_changed` for signal timeline tracking
- ✓ Array-based pin detection (`const int PIN[N] = {...}`)
- ✓ Multi-pin component grouping (HC-SR04 → DistanceSensor, H-bridge → HBridgeMotor, TCS3200 → ColorSensor)
- ✓ Motor (H-bridge) separated from Servo (PWM single pin)
- ✓ Canvas sensor inputs — distance (cm → µs), color (R/G/B 0-255), analog (0-1023)
- ✓ Servo angle display — live °label updated from analogWrite value
- ✓ `Servo` class — injected inline by `strip_includes()` replacing `#include <Servo.h>`
- ✓ Preprocessor `strip_includes()` step — runs before `replace_api_calls()`, handles library header replacement
- ✓ Temperature, light, and generic analog sensor canvas inputs
- ✓ HBridge motor PWM pin detection and speed display

> **Milestone:** Target benchmark sketch compiles and runs correctly. ✓

---

### Phase 4 — Board Profiles ✓

- ✓ `BoardProfile` struct in `src/core/runtime/boardprofile.h` — `name`, `chip`, `pin_count`, `analog_offset`, `analog_count`, `pwm_resolution`
- ✓ Built-in profiles: Arduino Uno (ATmega328P), Arduino Nano (ATmega328P), Arduino Mega 2560 (ATmega2560), Arduino Due (AT91SAM3X8E), Teensy 4.1 (IMXRT1062)
- ✓ Board selector in Settings dialog — saved to `board/name` in `settings.ini`
- ✓ `RuntimeState` pin arrays bumped to fixed `[80]` / `[20]` max, all hardcoded `20`/`14`/`8` replaced with profile values
- ✓ `inject_analog` / `impl_analogRead` use `profile.analog_offset` instead of hardcoded `14`
- ✓ `CanvasWidget` fully profile-aware — pin loops, pin spacing, `BOARD_H`, servo angle, board name and chip label on canvas graphic
- ✓ `setProfile()` chain: `SketchThread` → `SketchHost` → `ArduinoRuntime` — board change propagates to running runtime
- ✓ Unlocks running the full Lambo sketch on Teensy 4.1 without pin remapping
- ✓ `// @board <name>` sketch hint — `Preprocessor::extract_board_profile()` reads the comment from raw source, surfaced via `CompileResult::board_hint`, applied by MainWindow on run (canvas, label, runtime, and settings all update automatically)

---

### Phase 5 — Cross-Platform Support ✓

- ✓ Linux shared library — `sketch.so` compiled and loaded via `dlopen` / `dlsym` / `dlclose`
- ✓ Platform-abstracted DLL lifecycle — `#ifdef _WIN32` / `#else` guards in `SketchHost`
- ✓ Temp copy strategy consistent across platforms — `.tmp.dll` (Windows), `.tmp.so` (Linux)
- ✓ Linux compiler default — `/usr/bin/g++`, detected and pre-filled in settings dialog
- ✓ CMakeLists.txt links `dl` on Linux, no extra libs on Windows
- ✓ Build instructions for both platforms (CMake configure + build + run)

> **Milestone:** Full compile-run-stop cycle verified on both Windows (MinGW) and Linux. ✓

---

### Phase 6 — Arduino API Completion

Fill out all commonly-used Arduino API surface that is currently missing or only partially correct.

**Missing functions:**
- `tone(pin, frequency)` / `tone(pin, frequency, duration)` / `noTone(pin)` — buzzer/piezo support; no actual audio, just tracks state for canvas display
- `attachInterrupt(pin, ISR, mode)` — RISING, FALLING, CHANGE; ISR called synchronously when pin state changes in the simulation
- `EEPROM.read(addr)` / `EEPROM.write(addr, val)` / `EEPROM.update()` — 1024-byte simulated array, optional disk persistence between sessions
- `Serial1` / `Serial2` — additional hardware UARTs present on Mega, Due; same implementation as `Serial`, separate buffers

**Missing timing correctness:**
- `millis()` / `micros()` are currently wall-clock — make them advance by `elapsed * speed_multiplier` so timing-sensitive sketches (PID, debounce) behave correctly at non-1x speed

**Missing libraries (preprocessor injection, same approach as `Servo.h`):**
- `SoftwareSerial` — injected class, same buffer model as `Serial`
- `PROGMEM` / `F()` macro — `F("text")` and `PROGMEM` currently cause compile errors; inject no-op definitions so common sketches compile without modification
- `Wire.begin` / `Wire.write` / `Wire.read` — byte-level I2C simulation, virtual device responses; no electrical bus characteristics
- `SPI.begin` / `SPI.transfer` — same scope as Wire

**Missing components:**
- Keypad matrix — 4x4 or 3x4, detected from defines, clickable grid on canvas
- AVR hardware register simulation — `DDRB`, `PORTB`, `PINB`, etc. as overloaded-operator structs in injected header; reads/writes map to the same pin state as `digitalWrite`/`digitalRead`

> **Milestone:** The vast majority of real-world Arduino sketches compile and run without modification.

---

### Phase 7 — LCD Component

Add a working 16x2 LCD to the canvas. Rudimentary visuals only — characters displayed in a fixed-width grid, no pixel-accurate graphics. Pretty rendering comes later in Phase 12.

**How it works:**

The preprocessor injects a replacement `LiquidCrystal` class (same approach as `Servo.h`). The constructor `LiquidCrystal lcd(rs, en, d4, d5, d6, d7)` registers the LCD with the runtime using the actual pin numbers passed in — no naming conventions required. This makes it fully universal: any sketch using `#include <LiquidCrystal.h>` works automatically regardless of how the user named their pin variables.

**Work items:**
- Inject `LiquidCrystal` replacement class in `strip_includes()` — constructor registers pin mapping with runtime
- `lcd.begin(cols, rows)` — runtime stores display dimensions
- `lcd.print(val)` / `lcd.setCursor(col, row)` / `lcd.clear()` / `lcd.home()` — runtime maintains 16x2 character buffer
- Canvas renders the character buffer as a dark green box with monospace text — 16 characters × 2 rows
- Circuit detector LCD detection (already partially implemented) kept as fallback for sketches that don't include `LiquidCrystal.h`

> **Milestone:** A sketch using `LiquidCrystal` prints text and the canvas displays it correctly.

---

### Phase 8 — Multi-board Simulation

Run two boards simultaneously in the same session.

- Two `SketchThread` instances running at the same time
- Virtual serial pipe — TX of one board feeds RX of the other
- Both canvases visible simultaneously
- Enables master/slave, sensor node + controller, and I2C peripheral sketches

> **Milestone:** Two sketches communicate over virtual Serial and both canvases update correctly.

---

### Phase 9 — Memory Analysis

Give the user realistic flash and RAM usage figures without requiring a real AVR toolchain on the hot path.

- `avr_gcc_path` setting in settings dialog
- After successful compile, run `avr-gcc` for size analysis only (not execution)
- Parse `avr-size` output for flash and SRAM usage
- Flash → hard enforce: block Run if over board's flash limit
- Static RAM → hard enforce: block Run if globals exceed board's SRAM limit
- Dynamic RAM (String/malloc) → warn but don't block; track via `malloc`/`new` interception
- Memory bar in UI: `████░░░░ 1234 / 32256 bytes (3%)`
- Warn at >75% usage before hitting the limit
- Auto-detect Arduino IDE `avr-gcc` path on first run

> **Milestone:** Flash and RAM usage shown for every compile; over-limit sketches are blocked from running.

---

### Phase 10 — Simulation Realism + Canvas Improvements

Run these together — both are polish passes with no new component work.

**Simulation realism:**
- Floating pin simulation — undriven INPUT pins return random HIGH/LOW
- Button bounce simulation — rapid toggles on click before settling (~10ms)
- Optional gaussian noise on analog readings (off by default)

**Canvas improvements:**
- Canvas layout mode — "Layout" toolbar button, components become draggable
- Positions saved to `sketch_name.vblayout` next to `.cpp` file
- On load: use saved positions if file exists, otherwise auto-generate
- Wire visualization improvements — color-coded by signal type (digital, analog, PWM)

> **Milestone:** Simulation behaves realistically on edge cases; canvas layout can be saved and restored.

---

### Phase 11 — MicroPython / CircuitPython Support

Adds a Python execution path that plugs into the same runtime, canvas, and signal timeline as the existing C++ path. Board selection drives which path is used — the rest of the simulation is unchanged.

**How it works:**

```
Board = Raspberry Pi Pico (MicroPython)
    → BoardProfile.language == Language::MicroPython
    → Preprocessor replaces machine.Pin / utime calls with vb_runtime calls → writes temp.py
    → Python host (pybind11) executes temp.py
    → Same ArduinoRuntime state, same canvas callbacks
```

**Work items:**
- Add `Language` enum to `BoardProfile` (`Arduino`, `MicroPython`, `CircuitPython`)
- Raspberry Pi Pico board profile (`RP2040`, 30 pins, `Language::MicroPython`)
- MicroPython preprocessor replace table:

  | MicroPython | VEMCODE |
  |---|---|
  | `machine.Pin(N, OUT)` | `vb_pin_mode(N, OUT)` |
  | `pin.value(1)` | `vb_digital_write(N, 1)` |
  | `pin.value()` | `vb_digital_read(N)` |
  | `utime.sleep_ms(500)` | `vb_delay(500)` |
  | `utime.ticks_ms()` | `vb_millis()` |
  | `ADC(pin).read_u16()` | `vb_analog_read(pin)` |

- pybind11 integration — expose `ArduinoRuntime` as a Python module `vb_runtime`:
  ```cpp
  m.def("vb_digital_write", &impl_digitalWrite);
  m.def("vb_digital_read",  &impl_digitalRead);
  m.def("vb_delay",         &impl_delay);
  m.def("vb_millis",        &impl_millis);
  m.def("vb_analog_read",   &impl_analogRead);
  ```
  `temp.py` imports `vb_runtime` and calls back into the C++ host
- Python-aware circuit detector — scan for `machine.Pin(N` patterns instead of `#define` to detect components
- `SketchThread` Python execution path — alongside the existing DLL path, runs `temp.py` via embedded Python when `profile.language != Arduino`
- CircuitPython replace table after MicroPython works — different import names (`import board`, `digitalio`) but same GPIO concepts

**What stays the same across all boards:**
- `ArduinoRuntime` pin/analog state
- `CanvasWidget` and component interaction
- Signal timeline
- Variable watch
- `SketchThread` loop execution model

> **Milestone:** A MicroPython blink sketch runs on the Pico profile and toggles the canvas LED.

---

### Phase 12 — Full Component Visuals

Replace colored rectangles with proper component graphics across the board. This is the "make it pretty" pass deferred from all earlier phases.

- Proper graphics for all component types (sensor housings, chip outlines, etc.)
- LCD 16x2 — pixel-accurate character cell rendering, backlight color, cursor blink
- 7-segment display — single and multi-digit, segment-accurate rendering
- Basic OLED — text and simple graphics (SSD1306-compatible)
- NeoPixel / WS2812B strip — individually addressable RGB LEDs, single-pin protocol, configurable strip length

> **Milestone:** The canvas looks like a real breadboard layout with recognizable component graphics.

---

### Later

- macOS support
- Installer — bundle MinGW for zero-dependency install (Windows); package for common Linux distros
- Additional board profiles (ESP32, STM32) — add one `BoardProfile` entry each
