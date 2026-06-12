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

- ✓ `BoardProfile` struct in `src/core/runtime/boardprofile.h` — `name`, `chip`, `pin_count`, `analog_offset`, `analog_count`, `pwm_resolution`, `serial_count`
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

### Phase 6 — LCD Component ✓

Add a working 16x2 LCD to the canvas. Rudimentary visuals only — characters displayed in a fixed-width grid, no pixel-accurate graphics. Pretty rendering comes later in Phase 11.

**How it works:**

The preprocessor injects a replacement `LiquidCrystal` class in `strip_includes()` (same approach as `Servo.h`). The constructor stores the RS pin as the component identifier. `lcd.print()`, `lcd.clear()`, and `lcd.setCursor()` call `api->lcd_print(rs, row, text)` — a new entry in `ArduinoAPI` that fires a callback up through `ArduinoRuntime` → `SketchThread` → `CanvasWidget`, where `QGraphicsTextItem` labels on each row are updated in real time.

- ✓ `LiquidCrystal` replacement class injected by `strip_includes()` — `LiquidCrystal(rs, en, d4, d5, d6, d7)`, same approach as `Servo.h`
- ✓ `lcd.begin(cols, rows)` — signals LCD active via `digitalWrite(rs, HIGH)` and clears both rows
- ✓ `lcd.print(const char*)` / `lcd.print(String)` / `lcd.print(int)` / `lcd.print(float)` — all overloads call `api->lcd_print`
- ✓ `lcd.setCursor(col, row)` — tracks current row for subsequent `print()` calls
- ✓ `lcd.clear()` — clears both rows via `lcd_print`
- ✓ `lcd_print` API function — new entry at end of `ArduinoAPI` struct; `impl_lcd_print` in `ArduinoRuntime` fires `on_lcd_print` callback
- ✓ Qt signal chain — `on_lcd_print` → `emit lcdPrint(pin, row, text)` on `SketchThread` → `updateLcdText()` slot on `CanvasWidget`
- ✓ Canvas renders LCD as a cyan rectangle with two rows of `QGraphicsTextItem` (Courier New 7pt, 16 chars wide), keyed in `lcdRow0Labels_` / `lcdRow1Labels_` by RS pin
- ✓ CircuitDetector LCD detection — RS + EN + D4–D7 define group detected in `detect_multipin()`; RS pin used as representative; other 5 pins claimed to prevent duplicate single-pin entries

> **Milestone:** A sketch using `LiquidCrystal` prints text and the canvas displays it correctly. ✓

---

### Phase 7 — Arduino API Completion: Simple Surface + Simulation Realism

Fill out the remaining commonly-used Arduino API surface and add low-level simulation realism. All items are self-contained runtime or preprocessor changes with no inter-dependencies.

**Missing functions:**
- ✓ `tone(pin, frequency)` / `tone(pin, frequency, duration)` / `noTone(pin)` — buzzer/piezo support; no actual audio, just tracks state for canvas display
- ✓ `attachInterrupt(pin, ISR, mode)` — `RISING`, `FALLING`, `CHANGE` constants added to `vb` namespace; callback and mode stored in `RuntimeState`; `impl_attachInterrupt` registers the ISR but does not fire it automatically on pin changes — hardware interrupt simulation requires a CPU emulator and is outside the compile-to-native scope
- ✓ `noInterrupts()` / `interrupts()` — track enabled state in `RuntimeState::interrupts_enabled_`; preprocessor replaces calls with `api->` prefixed versions
- ✓ `EEPROM.read(addr)` / `EEPROM.write(addr, val)` / `EEPROM.update()` — 1024-byte `std::array<uint8_t, 1024>` in `RuntimeState`; bounds-checked (out-of-range returns `0xFF`); `update()` skips write if value unchanged; `#include <EEPROM.h>` stripped by preprocessor; no disk persistence between sessions
- ✓ `Serial1` / `Serial2` runtime — additional hardware UARTs on Mega 2560, Due, Teensy 4.1; same implementation as `Serial`, separate buffers and callbacks (`on_serial1_output`, `on_serial2_output`); preprocessor maps `Serial1.*` / `Serial2.*` calls to `api->Serial1_*` / `api->Serial2_*`
- ✓ `Serial1` / `Serial2` split monitor UI — when a board with `serial_count > 1` is active, the Serial monitor tab splits horizontally into labeled panes (Serial | Serial1 | Serial2); driven by `serial_count` on `BoardProfile`; `SketchThread` emits `serial1Output` / `serial2Output` signals wired to the new monitor panes; `rebuildSerialMonitors()` rebuilds the tab when the board profile changes at runtime

**Missing libraries (preprocessor injection, same approach as `Servo.h`):**
- ✓ `SoftwareSerial` — injected class replacing `#include <SoftwareSerial.h>`; constructor stores `rxPin`/`txPin`; `begin`, `print`/`println` (4 overloads each), `write(byte)`, `write(buf, n)`, `available`, `read`, `peek`; `listen`/`isListening`/`overflow` return stubs; output routed to main serial monitor prefixed `[SW:N]` where N is the RX pin; RX buffer injectable per-pin via `ArduinoRuntime::inject_soft_serial(rxPin, data)`; `replace_token()` preprocessor helper prevents variable names ending in `Serial` (e.g. `mySerial`) from being mis-rewritten by the `Serial.*` replacement pass

**Missing sketch structure:**
- ✓ Multi-file sketch support — if a sketch folder contains `.h` or additional `.cpp` files, include them in the compile pass; `strip_includes()` must pass through `#include "localfile.h"` rather than stripping it

**New components:**
- RGB LED — three PWM pins (R/G/B), detected from `#define` pin names; canvas shows a colored circle that blends the three channel values in real time
- Rotary encoder — two digital pins (CLK/DT) plus optional button pin; canvas shows a turn dial; pairs with `attachInterrupt` for accurate pulse counting

**Serial Plotter:**
- Numeric values printed via `Serial.println()` graphed over time in a scrolling plot panel alongside the serial monitor; multiple named variables supported via `Serial.print("label:"); Serial.println(value);` format, matching the Arduino IDE Serial Plotter protocol

**Simulation realism:**
- Floating pin simulation — undriven INPUT pins return random HIGH/LOW
- Button bounce simulation — rapid toggles on click before settling (~10ms)
- Optional gaussian noise on analog readings (off by default)

> **Milestone:** Simple sketches using timers, interrupts, EEPROM, and additional serial ports run correctly; the simulation behaves realistically on common hardware edge cases.

---

### Phase 8 — Arduino API Completion: Protocol Libraries + Complex Components

Heavier API and component work requiring more architectural changes: bus protocol simulation, virtual device responses, and complex canvas components.

**Protocol libraries (preprocessor injection + virtual device responses):**
- `Wire.begin` / `Wire.write` / `Wire.read` — byte-level I2C simulation, virtual device responses; no electrical bus characteristics
- `SPI.begin` / `SPI.transfer` — same scope as Wire

**Complex components:**
- Joystick — two analog axes (X/Y, 0–1023) plus a digital button; detected from `#define` pin names; canvas shows dual sliders and a clickable button
- Stepper motor — step count and direction tracked from STEP/DIR or IN1-IN4 pin patterns; canvas displays a position counter and rotation indicator
- Keypad matrix — 4x4 or 3x4, detected from defines, clickable grid on canvas
- AVR hardware register simulation — `DDRB`, `PORTB`, `PINB`, etc. as overloaded-operator structs in injected header; reads/writes map to the same pin state as `digitalWrite`/`digitalRead`

> **Milestone:** Sketches using I2C/SPI sensor libraries compile and run; joystick, keypad, and direct register access all work on the canvas.

---

### Phase 9 — Editor Improvements

Polish the editor into a first-class coding environment. All items are self-contained UI work with no runtime dependencies.

- **Code completion** — Ctrl+Space shows a filtered popup of Arduino API functions plus all functions, variables, and `#define` constants declared in the current sketch
- **Find & Replace** — Ctrl+F opens an inline find bar; Ctrl+H adds a replace field; Enter steps through matches, Escape dismisses
- **Save in-place** — Ctrl+S saves silently to the current file path when a sketch is already open; only prompts for a name on first save of a new unsaved sketch
- **Unsaved changes indicator** — append `*` to the window title when the editor content differs from the saved file; clear it on save
- **Auto-close brackets** — typing `(`, `[`, `{`, or `"` inserts the matching closer and positions the cursor inside; typing the closer when it is the next character skips over it instead of doubling
- **Bracket matching** — when the cursor sits adjacent to `(`, `)`, `{`, `}`, `[`, or `]`, highlight the matching bracket
- **Comment toggle** — Ctrl+/ adds `// ` to the current line or selected lines; pressing again removes it
- **Font size zoom** — Ctrl+`+` / Ctrl+`-` / Ctrl+scroll adjusts the editor font size; resets to default with Ctrl+`0`
- **Duplicate line** — Ctrl+D copies the current line and inserts it on the line below
- **Compile warnings** — compiler warnings surfaced in the editor alongside errors; yellow line backgrounds for warning lines with corrected line numbers
- **Sketch templates** — "New Sketch" dialog offers built-in starters (Blink, Button, Serial Echo, Servo Sweep, Analog Read, PID loop); selected template copied into the new sketch folder

> **Milestone:** The editor feels complete for day-to-day sketch writing with no obvious missing shortcuts.

---

### Phase 10 — Memory Analysis

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
- Export `.hex` — toolbar button generates a flashable `.hex` for real hardware once avr-gcc is configured; output placed in the sketch subfolder alongside the compiled DLL

> **Milestone:** Flash and RAM usage shown for every compile; over-limit sketches are blocked from running; `.hex` can be exported for flashing to real hardware.

---

> ### Beta Release
> Tag `v1.0-beta` after Phase 10 is complete. The tool is feature-complete enough for real-world use: the editor is polished, the API surface covers the common cases, and users get meaningful memory feedback. Installer and auto-update (QtIFW + GitHub Releases as update repository) ships with the stable `v1.0` release — not before.

---

### Phase 11 — Component Visual Upgrades + Canvas Improvements

Replace colored rectangles with proper component graphics and give the canvas a layout system. Both are polish passes on existing functionality with no new runtime work.

**Component visual upgrades:**
- Refactor `CanvasWidget` to use typed `ComponentItem` subclasses (inheriting `QGraphicsObject`) instead of raw `QGraphicsRectItem` — each component type owns its `paint()` and `setState()`, giving `CanvasWidget` a uniform interface regardless of how a component renders itself
- All component visuals implemented as QPainter-drawn shapes in the initial pass — no SVG assets required; animations (LED glow, buzzer pulse, motor rotation) driven by a shared canvas `QTimer` incrementing a `phase_` value on each active item
- Architecture is SVG-ready by design: swapping a component's visuals to `QSvgRenderer`-based rendering later only requires changing that component's `paint()` implementation; nothing in `CanvasWidget` or the signal wiring changes
- LCD 16x2 — pixel-accurate character cell rendering, backlight color, cursor blink

**Canvas improvements:**
- Canvas layout mode — "Layout" toolbar button, components become draggable
- Positions saved to `sketch_name.vblayout` next to `.cpp` file
- On load: use saved positions if file exists, otherwise auto-generate
- Wire visualization improvements — color-coded by signal type (digital, analog, PWM)

> **Milestone:** The canvas looks polished with recognizable component graphics; layout can be saved and restored. Adding SVG art for any component in the future is a single-file `paint()` swap.

---

### Phase 12 — New Display Components

Add new output components with full runtime and canvas support. Each requires a new injected library class, runtime state, and canvas widget.

- 7-segment display — single and multi-digit, segment-accurate rendering
- Basic OLED — text and simple graphics (SSD1306-compatible)
- NeoPixel / WS2812B strip — individually addressable RGB LEDs, single-pin protocol, configurable strip length

> **Milestone:** Sketches using 7-segment displays, OLEDs, and NeoPixel strips render correctly on the canvas.

---

### Phase 13 — Multi-board Simulation

Run two boards simultaneously in the same session.

- Two `SketchThread` instances running at the same time
- Thread-safe state injection — replace `pin_values`, `analog_values`, and `pwm_values` arrays in `RuntimeState` with `std::atomic<int>` so UI inject calls and both sketch threads can't race on shared state
- Virtual serial pipe — TX of one board feeds RX of the other
- Both canvases visible simultaneously
- Enables master/slave, sensor node + controller, and I2C peripheral sketches

> **Milestone:** Two sketches communicate over virtual Serial and both canvases update correctly.

---

### Phase 14 — MicroPython / CircuitPython Support

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

### Later (post-stable)

- Installer — QtIFW with GitHub Releases as the update repository; bundle MinGW for zero-dependency install on Windows, package for common Linux distros
- macOS support
- Additional board profiles (ESP32, STM32) — add one `BoardProfile` entry each
- Step-through debugger — pause execution at a line and step statement by statement; implemented by injecting `vb_breakpoint(line)` calls in the preprocessor and blocking on a condition variable when paused; UI adds clickable gutter breakpoints and a step/resume toolbar
- Custom SVG component visuals — replace QPainter-drawn component art with `QSvgRenderer`-based rendering; each component type is a split SVG (static housing layer + animated overlay layer composited via QPainter transforms); static parts designed in Inkscape/Figma and embedded as Qt resources; animated parts (motor spinner, servo arm, LED glow) remain QPainter-driven on top of the SVG base; enabled by the `ComponentItem` subclass architecture from Phase 11 — each component's upgrade is a self-contained `paint()` change
