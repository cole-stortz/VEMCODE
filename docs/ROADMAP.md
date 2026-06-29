# VEMCODE : Roadmap

Completed items are marked `[x]`. Active and future phases are in order of planned work.

---

### Phase 0 — Core Infrastructure ✓

- [x] Initial file structure and CMakeLists.txt
- [x] `ArduinoAPI` function pointer table — all Arduino calls go through injected struct
- [x] `ArduinoRuntime` — implements all `impl_*` functions, owns simulation state
- [x] `SketchThread` — QThread running `vb_loop()` on a background thread
- [x] First working Qt6 GUI with serial monitor output
- [x] AutoCompile pipeline — file watch → g++ invocation → DLL hot-reload
- [x] Output DLL placed in sketch subfolder (not build dir)
- [x] Initial Preprocessor — transforms Arduino source to DLL format (`vb_init`, `vb_setup`, `vb_loop`)
- [x] Circuit canvas (`CanvasWidget`) with basic component rendering
- [x] Clickable Button component on canvas
- [x] `CircuitDetector` keyword scan — detects component types from `#define` names
- [x] Delay consistency — simulated delay tracks sketch timing

> **Milestone:** "Blink" sketch compiles, loads, and toggles the LED on the canvas. ✓

---

### Phase 1 — Editor and Language Features ✓

- [x] Syntax highlighting — blue keywords, yellow functions, green comments, orange strings
- [x] Compile error highlighting — red line backgrounds via `QTextEdit::ExtraSelection`
- [x] Error line number correction — subtracts `INJECTED_HEADER_LINES` so errors point to user sketch lines
- [x] Corrected error message names (temp file path stripped, `api->` prefix stripped)
- [x] Line number gutter — `EditorWithLines` + `LineNumberArea` subclass
- [x] Auto-indent and auto-dedent — Enter carries indentation, Tab inserts 4 spaces, `}` dedents
- [x] Variable watch panel — `QTableWidget` updated from `watch_variable()` callbacks
- [x] `Serial.println` type overloads — int, float, String, const char*
- [x] `String` class — wraps `std::string`, injected into preprocessor header
- [x] Math functions — `map()`, `constrain()`, `abs()`, `min()`, `max()`, `random()`
- [x] Safety delay injection — preprocessor inserts `api->delay(10)` if no delay found in `loop()`, prevents infinite loop crash

> **Milestone:** Non-trivial sketches using String and math helpers compile and run without crashes. ✓

---

### Phase 2 — UI Polish and User Workflow ✓

- [x] First-run settings dialog — compiler path and project root saved to `app/settings.ini`
- [x] New Sketch button — creates empty sketch in a new subfolder
- [x] Recent Sketches button — last 5 paths persisted in `settings.ini`
- [x] Speed slider — range 1–25 (= 0.1x–2.5x), passed to runtime as `speed_multiplier = 1/speed`
- [x] Stop delay fix — `impl_delay` sleeps in 10ms chunks, checks `stop_requested_` between each chunk
- [x] `Serial.available()` / `Serial.read()` — UI text input feeds `serial_buffer_`, consumed by runtime
- [x] Switch component — toggles state on click, persists in `switchStates_` QMap
- [x] Potentiometer component — drag up/down changes analog value 0–1023
- [x] Two-column canvas layout — inputs left, outputs right, pin-aligned wiring
- [x] Signal timeline — logic analyzer waveform view for digital pin state history

> **Milestone:** Full interactive sketch workflow: open, edit, compile, run, adjust inputs, stop. ✓

---

### Phase 3 — Component Completion ✓

- [x] `pulseIn(pin, value, timeout)` — fast path (distance sensor), color channel path (TCS3200), slow path (pin polling)
- [x] `delayMicroseconds` — busy-wait with stop check
- [x] `analogWrite` fires `on_pin_changed` for signal timeline tracking
- [x] Array-based pin detection (`const int PIN[N] = {...}`)
- [x] Multi-pin component grouping (HC-SR04 → DistanceSensor, H-bridge → HBridgeMotor, TCS3200 → ColorSensor)
- [x] Motor (H-bridge) separated from Servo (PWM single pin)
- [x] Canvas sensor inputs — distance (cm → µs), color (R/G/B 0-255), analog (0-1023)
- [x] Servo angle display — live °label updated from analogWrite value
- [x] `Servo` class — injected inline by `strip_includes()` replacing `#include <Servo.h>`
- [x] Preprocessor `strip_includes()` step — runs before `replace_api_calls()`, handles library header replacement
- [x] Temperature, light, and generic analog sensor canvas inputs
- [x] HBridge motor PWM pin detection and speed display

> **Milestone:** Target benchmark sketch compiles and runs correctly. ✓

---

### Phase 4 — Board Profiles ✓

- [x] `BoardProfile` struct in `src/core/runtime/boardprofile.h` — `name`, `chip`, `pin_count`, `analog_offset`, `analog_count`, `pwm_resolution`, `serial_count`
- [x] Built-in profiles: Arduino Uno (ATmega328P), Arduino Nano (ATmega328P), Arduino Mega 2560 (ATmega2560), Arduino Due (AT91SAM3X8E), Teensy 4.1 (IMXRT1062)
- [x] Board selector in Settings dialog — saved to `board/name` in `settings.ini`
- [x] `RuntimeState` pin arrays bumped to fixed `[80]` / `[20]` max, all hardcoded `20`/`14`/`8` replaced with profile values
- [x] `inject_analog` / `impl_analogRead` use `profile.analog_offset` instead of hardcoded `14`
- [x] `CanvasWidget` fully profile-aware — pin loops, pin spacing, `BOARD_H`, servo angle, board name and chip label on canvas graphic
- [x] `setProfile()` chain: `SketchThread` → `SketchHost` → `ArduinoRuntime` — board change propagates to running runtime
- [x] Unlocks running the full Lambo sketch on Teensy 4.1 without pin remapping
- [x] `// @board <name>` sketch hint — `Preprocessor::extract_board_profile()` reads the comment from raw source, surfaced via `CompileResult::board_hint`, applied by MainWindow on run (canvas, label, runtime, and settings all update automatically)

---

### Phase 5 — Cross-Platform Support ✓

- [x] Linux shared library — `sketch.so` compiled and loaded via `dlopen` / `dlsym` / `dlclose`
- [x] Platform-abstracted DLL lifecycle — `#ifdef _WIN32` / `#else` guards in `SketchHost`
- [x] Temp copy strategy consistent across platforms — `.tmp.dll` (Windows), `.tmp.so` (Linux)
- [x] Linux compiler default — `/usr/bin/g++`, detected and pre-filled in settings dialog
- [x] CMakeLists.txt links `dl` on Linux, no extra libs on Windows
- [x] Build instructions for both platforms (CMake configure + build + run)

> **Milestone:** Full compile-run-stop cycle verified on both Windows (MinGW) and Linux. ✓

---

### Phase 6 — LCD Component ✓

Add a working 16x2 LCD to the canvas. Rudimentary visuals only — characters displayed in a fixed-width grid, no pixel-accurate graphics. Pretty rendering comes later in Phase 11.

**How it works:**

The preprocessor injects a replacement `LiquidCrystal` class in `strip_includes()` (same approach as `Servo.h`). The constructor stores the RS pin as the component identifier. `lcd.print()`, `lcd.clear()`, and `lcd.setCursor()` call `api->lcd_print(rs, row, text)` — a new entry in `ArduinoAPI` that fires a callback up through `ArduinoRuntime` → `SketchThread` → `CanvasWidget`, where `QGraphicsTextItem` labels on each row are updated in real time.

- [x] `LiquidCrystal` replacement class injected by `strip_includes()` — `LiquidCrystal(rs, en, d4, d5, d6, d7)`, same approach as `Servo.h`
- [x] `lcd.begin(cols, rows)` — signals LCD active via `digitalWrite(rs, HIGH)` and clears both rows
- [x] `lcd.print(const char*)` / `lcd.print(String)` / `lcd.print(int)` / `lcd.print(float)` — all overloads call `api->lcd_print`
- [x] `lcd.setCursor(col, row)` — tracks current row for subsequent `print()` calls
- [x] `lcd.clear()` — clears both rows via `lcd_print`
- [x] `lcd_print` API function — new entry at end of `ArduinoAPI` struct; `impl_lcd_print` in `ArduinoRuntime` fires `on_lcd_print` callback
- [x] Qt signal chain — `on_lcd_print` → `emit lcdPrint(pin, row, text)` on `SketchThread` → `updateLcdText()` slot on `CanvasWidget`
- [x] Canvas renders LCD as a cyan rectangle with two rows of `QGraphicsTextItem` (Courier New 7pt, 16 chars wide), keyed in `lcdRow0Labels_` / `lcdRow1Labels_` by RS pin
- [x] CircuitDetector LCD detection — RS + EN + D4–D7 define group detected in `detect_multipin()`; RS pin used as representative; other 5 pins claimed to prevent duplicate single-pin entries

> **Milestone:** A sketch using `LiquidCrystal` prints text and the canvas displays it correctly. ✓

---

### Phase 7 — Arduino API Completion: Simple Surface + Simulation Realism

Fill out the remaining commonly-used Arduino API surface and add low-level simulation realism. All items are self-contained runtime or preprocessor changes with no inter-dependencies.

**Missing functions:**
- [x] `tone(pin, frequency)` / `tone(pin, frequency, duration)` / `noTone(pin)` — buzzer/piezo support; no actual audio, just tracks state for canvas display
- [x] `attachInterrupt(pin, ISR, mode)` — `RISING`, `FALLING`, `CHANGE` constants added to `vb` namespace; callback and mode stored in `RuntimeState`; `impl_attachInterrupt` registers the ISR and `impl_digitalWrite` fires it on matching pin transitions
- [x] ISR dispatch — `impl_digitalWrite` checks `RuntimeState` for any ISR registered on the target pin after updating its state; if the transition matches the registered mode (`RISING`: LOW→HIGH, `FALLING`: HIGH→LOW, `CHANGE`: either), the ISR function pointer is called directly on the sketch thread; `interrupts_enabled_` is checked first and the call is skipped if `noInterrupts()` is active; the dispatcher temporarily sets `interrupts_enabled_ = false` before calling the ISR and restores it after, matching AVR's automatic cli/sei behaviour around interrupt execution; logically correct for rotary encoders, pulse counters, and interrupt-driven sensors even without cycle-accurate AVR timing
- [x] `ISR()` vector macro transform — preprocessor scans for `ISR(X_vect) { ... }` blocks before compilation; strips the AVR-specific macro wrapper, renames the function to `__vb_isr_X_vect()`, and injects `api->register_isr("X_vect", __vb_isr_X_vect)` calls into `vb_setup()`; `register_isr` stores handlers in `RuntimeState::isr_handlers_`; `avr/interrupt.h` and `avr/io.h` stripped silently; supported vectors and their simulation triggers:
  - [x] `INT0_vect` / `INT1_vect` → pin 2 / pin 3 transition; dispatched from `impl_digitalWrite`
  - [x] `PCINT0_vect` / `PCINT1_vect` / `PCINT2_vect` → pin-change group transitions; dispatched from `impl_digitalWrite`
  - [ ] `TIMER1_OVF_vect` / `TIMER2_OVF_vect` → timer overflow from Phase 9 timer register simulation
  - [ ] `TIMER1_COMPA_vect` / `TIMER1_COMPB_vect` → timer compare-match A/B (`OCR1A` / `OCR1B`)
  - [x] `USART_RX_vect` → fires when the user sends input via the serial monitor (`inject_serial`)
  - [x] `WDT_vect` → watchdog timeout in interrupt mode (rather than triggering a reset); coexists with the `avr/wdt.h` simulation
  - [x] Unknown vectors → surfaced as a warning: *"ISR vector 'X_vect' is not simulated — the handler will never fire"* rather than a silent compile failure
- [x] `noInterrupts()` / `interrupts()` — track enabled state in `RuntimeState::interrupts_enabled_`; preprocessor replaces calls with `api->` prefixed versions
- [x] `EEPROM.read(addr)` / `EEPROM.write(addr, val)` / `EEPROM.update()` — 1024-byte `std::array<uint8_t, 1024>` in `RuntimeState`; bounds-checked (out-of-range returns `0xFF`); `update()` skips write if value unchanged; `#include <EEPROM.h>` stripped by preprocessor; no disk persistence between sessions
- [x] `Serial1` / `Serial2` runtime — additional hardware UARTs on Mega 2560, Due, Teensy 4.1; same implementation as `Serial`, separate buffers and callbacks (`on_serial1_output`, `on_serial2_output`); preprocessor maps `Serial1.*` / `Serial2.*` calls to `api->Serial1_*` / `api->Serial2_*`
- [x] `Serial1` / `Serial2` split monitor UI — when a board with `serial_count > 1` is active, the Serial monitor tab splits horizontally into labeled panes (Serial | Serial1 | Serial2); driven by `serial_count` on `BoardProfile`; `SketchThread` emits `serial1Output` / `serial2Output` signals wired to the new monitor panes; `rebuildSerialMonitors()` rebuilds the tab when the board profile changes at runtime
- [ ] `Serial.printf(format, ...)` — common on ARM and ESP32 boards; injected via the preprocessor as an overload on the `Serial` object; maps to `snprintf` into a stack buffer then calls `api->serial_print`; `#include <stdio.h>` already available in the injected header

**Missing libraries (preprocessor injection, same approach as `Servo.h`):**
- [x] `SoftwareSerial` — injected class replacing `#include <SoftwareSerial.h>`; constructor stores `rxPin`/`txPin`; `begin`, `print`/`println` (4 overloads each), `write(byte)`, `write(buf, n)`, `available`, `read`, `peek`; `listen`/`isListening`/`overflow` return stubs; output routed to main serial monitor prefixed `[SW:N]` where N is the RX pin; RX buffer injectable per-pin via `ArduinoRuntime::inject_soft_serial(rxPin, data)`; `replace_token()` preprocessor helper prevents variable names ending in `Serial` (e.g. `mySerial`) from being mis-rewritten by the `Serial.*` replacement pass
- [x] Library injection files — each injected library class lives in its own `.inc` file in `src/core/build/libs/` (`servo.inc`, `liquidcrystal.inc`, `softwareserial.inc`), embedded at build time the same way `injected_header.inc` is; `strip_includes()` is a flat table of `{header_name, const char* content}` pairs and a single loop — adding a new injectable library = add one `.inc` file, embed it in CMake, add one entry to the table
- [x] `avr/wdt.h` — watchdog timer simulation; `wdt_enable(WDTO_Xs)` starts a countdown timer in `RuntimeState` (timeout values: WDTO_15MS through WDTO_8S); `wdt_reset()` resets the countdown; if the timer expires before the next `wdt_reset()` call, the simulation triggers a virtual reset (stops the sketch thread, clears runtime state, restarts from `vb_setup()`) and surfaces a canvas message *"Watchdog reset — wdt_reset() was not called in time"*; when combined with sleep modes, watchdog expiry is the wakeup condition; `wdt_disable()` cancels the timer; injected header defines `WDTO_*` constants matching real AVR values
- [x] `avr/sleep.h` — sleep mode simulation; `set_sleep_mode(mode)` stores the requested mode in `RuntimeState` (`SLEEP_MODE_IDLE`, `SLEEP_MODE_PWR_SAVE`, `SLEEP_MODE_PWR_DOWN`, etc.); `sleep_enable()` sets a flag; `sleep_cpu()` blocks the sketch thread on a condition variable — the thread suspends and the canvas shows a *"Sleeping…"* indicator; wakeup sources release the condition variable: watchdog timer expiry (any sleep mode) or ISR dispatch firing on a pin configured with `attachInterrupt` (modes that support pin-change wakeup); `sleep_disable()` clears the flag; covers the common pattern of `wdt_enable` → `sleep_cpu()` → periodic wakeup used in battery-powered data loggers and low-power sketches

**Missing sketch structure:**
- [x] Multi-file sketch support — if a sketch folder contains `.h` or additional `.cpp` files, include them in the compile pass; `strip_includes()` must pass through `#include "localfile.h"` rather than stripping it
- [x] Safety delay injection in `while` loops — `inject_while_delays()` scans every `while(...) { }` body and injects `api->delay(1)` if no delay is present; skips `do...while` tails and bodies that already have delays; tight sensor-polling loops no longer freeze the simulation thread
- [x] `F()` macro compatibility — `F("string")` is used in a large proportion of real sketches to store string literals in AVR flash; in VEMCODE on x86 there is no flash distinction, so `F(x)` should be defined as `(x)` in the injected header; without this, any sketch using `F()` fails to compile with a cryptic error
- [x] Inline AVR assembly transform — `transform_asm_blocks()` runs early in the pipeline; handles `__asm__`, `asm`, with or without `__volatile__`/`volatile`, with or without constraint strings:
  - [x] `__asm__("nop")` → stripped silently
  - [x] `__asm__("cli")` → `api->noInterrupts()`
  - [x] `__asm__("sei")` → `api->interrupts()`
  - [x] `__asm__("sleep")` → stripped with note
  - [x] `__asm__("wdr")` → stripped with note
  - [x] `__asm__("rjmp 0")` → stripped with note
  - [x] Unrecognized instruction → stripped with warning: *"Unrecognized assembly instruction 'X' removed"*
- [x] `PROGMEM` keyword compatibility — `const char text[] PROGMEM = "..."` is common in real sketches for flash storage; `PROGMEM` is an AVR-specific GCC attribute that doesn't exist on x86; define it as empty (`#define PROGMEM`) in the injected header so sketches using it compile without errors
- [x] `#ifdef ARDUINO` / `#ifndef ARDUINO` — common pattern in cross-platform sketches that lets code detect whether it's running on real hardware; VEMCODE doesn't define `ARDUINO` so the wrong branch compiles; fix is one line in the injected header: `#define ARDUINO 100` (matching the value the real Arduino IDE defines)
- [x] `pgm_read_byte` / `pgm_read_word` / `pgm_read_dword` / `pgm_read_float` — plain pointer dereferences in the injected header; `#include <avr/pgmspace.h>` stripped silently
- [x] `<util/delay.h>` — `#define F_CPU 16000000UL`, `#define _delay_ms(ms) api->delay(...)`, `#define _delay_us(us) api->delayMicroseconds(...)`; `#include <util/delay.h>` stripped silently
- [x] `analogReference(DEFAULT/INTERNAL/EXTERNAL)` — stubbed as a no-op in the injected header

**Error UX:**
- [x] Humanized compiler errors — post-process raw g++ output before display; a regex rewrite table maps common cryptic patterns to plain-English messages:
  - `'X' was not declared in this scope` → `"'X' not found — did you forget to declare it?"`
  - `no matching function for call to 'X'` → `"Wrong arguments passed to X"`
  - `expected ';' before '}'` → `"Missing semicolon, probably the line above"`
  - `expected '}' at end of input` → `"Unclosed brace — one of your { was never closed"`
  - `lvalue required as left operand of assignment` → `"Did you mean == instead of =?"`
  - `undefined reference to 'X'` → `"Function 'X' is used but never defined"`
  - `control reaches end of non-void function` → `"Function is missing a return statement"`
  - `expected unqualified-id before '{'` → `"Code found outside a function — all code must be inside setup(), loop(), or another function"`
  - `too many/few arguments to function 'X'` → `"Wrong number of arguments passed to 'X'"`
  - `stray '\' in program` → `"Invalid character in code — this sometimes happens when copy-pasting from a website; try retyping the line"`
  - `overflow in implicit constant conversion` → `"Number is too large for this variable type — try using long instead of int"`
  - `comparison between pointer and integer` → `"Can't compare strings with == — use strcmp() or the String class"`
- [x] No-components-detected hint — after `CircuitDetector::detect()` runs, if `components_` is empty (or contains only a Serial entry), the reason matters and the message should reflect it; three distinct cases:
  - Pin definitions found but names not recognized as component keywords (e.g. `#define MY_OUTPUT 5`) → *"Pin definitions found but couldn't identify component types — try descriptive names like `LED_PIN`, `SERVO_PIN`, `BUTTON_PIN`"*
  - Hardcoded pin numbers used with no defines at all (e.g. `digitalWrite(5, HIGH)`) → *"Pin numbers are hardcoded — give them names like `const int LED_PIN = 5;` so the simulator can identify them"*
  - Pin definitions exist only in an included local header (e.g. `#include "config.h"` has the `#define`s) — `CircuitDetector` currently only scans the main `.cpp`; extend it to also scan local `.h` files pulled in by the sketch, or surface: *"No components detected — if your pin definitions are in a header file, try moving them into the main sketch"*
  - No pin usage detected at all → existing generic message
- [x] Unsupported `#include` warning — after known headers are replaced, any remaining `#include <X.h>` generates a named warning in the serial monitor before compile: *"WARNING: \<Wire.h\> is not supported by VEMCODE — calls to this library will not work"*
- [x] Missing `setup()` / `loop()` — regex-checked before invoking g++; surfaces *"Sketch is missing a setup() function"* / *"…loop() function"* instead of a wall of linker errors
- [x] Pin out of range for selected board — if a `const int` or `#define` pin value exceeds the active board's pin count, warn: *"Pin 50 is not available on the Arduino Uno (max pin 13)"*
- [x] `analogWrite()` on a non-PWM pin — cross-reference `analogWrite` call sites against the board profile's PWM pin list and warn: *"Pin X does not support PWM on the selected board — analogWrite() will have no effect"*
- [x] Same pin claimed by two components — when `CircuitDetector` would silently drop a duplicate, instead surface: *"Pin X is used by both [Component A] and [Component B] — only one will be simulated"*
- [x] `// @board` hint unrecognised — if `extract_board_profile()` finds a `// @board` comment but the name doesn't match any known profile, warn: *"Unknown board 'X' in @board hint — using currently selected board instead"*
- [x] `map()` with equal min/max — static check for `map(val, x, x, ...)` or runtime divide-by-zero guard in `impl_map`; surface *"map() called with min == max — this causes a division by zero"* instead of a silent crash
- [x] Sketch thread crash wrapper — wrap the sketch execution loop in a try/catch and install a SIGFPE/SIGSEGV handler so any unhandled exception, division by zero, or out-of-bounds crash surfaces *"Sketch crashed — check for division by zero or out-of-bounds array access"* instead of a silently frozen canvas
- [x] `delay()` inside ISR callback — static check: if a `delay()` call appears inside a function registered via `attachInterrupt()`, warn *"delay() inside an interrupt handler will hang on real Arduino — interrupts are disabled during ISR execution"*
- [x] `digitalPinToInterrupt(pin)` defined in injected header as `inline int digitalPinToInterrupt(int pin) { return pin; }` so sketches using it correctly compile without error
- [x] Pin defined as an expression — `#define LED_PIN (2+1)` or `const int LED_PIN = BASE + 3;` compiles and runs fine but `CircuitDetector` cannot evaluate the expression and silently misses the component; detect when a pin define contains operators or references another variable and warn: *"Pin 'LED_PIN' is defined as an expression — the simulator could not evaluate it and the component may not appear on the canvas; use a plain number instead"*

**Simulation accuracy warnings** *(patterns that work in VEMCODE but fail on real hardware):*
- [x] Missing `volatile` on ISR-shared variables — if a variable is written inside an `attachInterrupt` callback and read in `loop()` or `setup()`, warn: *"'X' is shared with an ISR but not declared volatile — this may work in simulation but will likely fail on real hardware"*
- [x] `String +=` in a tight loop — if `String` concatenation is detected inside `loop()` with no apparent upper bound, warn: *"Repeated String concatenation in loop() causes heap fragmentation on real Arduino — consider using a char buffer instead"*
- [x] `pinMode()` never called for a `digitalWrite()` pin — if a pin appears in a `digitalWrite()` call but has no corresponding `pinMode(pin, OUTPUT)`, warn: *"Pin X is used with digitalWrite() but never set as OUTPUT via pinMode() — it will default to INPUT on real hardware"*

**Simulation realism:**
- [x] Floating pin simulation — undriven INPUT pins return random HIGH/LOW
- [x] Button bounce simulation — rapid toggles on click before settling (~10ms); `TACT`/`CLEAN`/`IDEAL` prefix gives a `ButtonClean` component with no bounce
- [x] Optional gaussian noise on analog readings (off by default, toggle in Settings dialog)

> **Milestone:** Simple sketches using timers, interrupts, EEPROM, and additional serial ports run correctly; the simulation behaves realistically on common hardware edge cases.

---

### Phase 8 — Component Plugin System + Generator + New Components

Pull the component plugin architecture forward so that all new components added in this phase and beyond use the new system from day one. The dev component generator is built on top of the clean plugin architecture so that adding new components never requires manually touching `CircuitDetector`, `CanvasWidget`, or any of the seven files in the adding-components checklist.

**Plugin system refactor:**
- [ ] `ComponentDefinition` struct in `src/core/circuit/componentregistry.h` — holds `type`, `detect_single` keyword list, `detect_multi` pin-role map, `detect_pattern` source pattern list, `min_pins`/`max_pins`, and `create_item` factory function
- [ ] `ComponentRegistry` singleton in `src/core/circuit/componentregistry.cpp` — flat list of definitions; all component files self-register at static init time
- [ ] `ComponentItem` base class inheriting `QGraphicsObject` — defines the `paint()` and `setState(QVariant)` interface; initial visuals are the same colored rectangles as today — no visual regression, just structural
- [ ] `CircuitDetector` refactored to loop over the registry instead of hardcoded if-chains — becomes a generic detection engine with no component-specific knowledge; detection runs in three confidence tiers in order: (1) `detect_pattern` source patterns first — library method calls and characteristic multi-call sequences that unambiguously identify a component (e.g. `myServo.attach(`, `lcd.begin(`, `pulseIn(` paired with a trig/echo timing sequence); (2) `detect_multi` pin-role matching for multi-pin components whose patterns didn't fire; (3) `detect_single` keyword matching against `#define` names as the final fallback — a match at a higher tier short-circuits the lower tiers so a sketch using `Servo.attach()` gets a Servo regardless of what the pin is named
- [ ] `CanvasWidget` refactored to call `def.create_item(pin, parent)` from the registry — no per-type switch blocks
- [ ] Each existing component migrated to its own file in `src/components/` (e.g. `led.cpp`, `servo.cpp`, `button.cpp`, `distancesensor.cpp`, `lcd.cpp`, etc.) — components that already benefit from pattern detection (Servo via `.attach(`, LCD via `LiquidCrystal` constructor, HC-SR04 via `pulseIn` + trig/echo sequence) get their `detect_pattern` entries written during migration
- [ ] `CMakeLists.txt` updated to glob `src/components/*.cpp` — new component files picked up by the build automatically with no CMake edits

**Dev component generator:**

A dev-only panel accessible under Settings → Developer Tools (hidden in normal builds, visible when VEMCODE is built in dev mode via a CMake flag). Fills out a form and writes all necessary files and edits automatically.

The generator UI has four sections:

- [ ] **Identity** — component name, display label, and detection keywords (comma-separated); single-pin or multi-pin toggle; if multi-pin, a list of named pin roles (e.g. TRIG, ECHO) each with their own keyword list and an `infer_type` flag marking which role is the representative pin
- [ ] **Detection patterns** — optional list of source code patterns that identify this component with high confidence regardless of pin naming; each entry is a string or sequence of strings that must all appear in the sketch source (e.g. `.attach(` for Servo, `lcd.begin(` for LCD); patterns are checked before keyword matching and a pattern match takes priority; leaving this section empty falls back to keyword-only detection
- [ ] **Interaction** — what the user can do with the component on the canvas; options are None (output only), Click (button/switch style), Click and Drag (potentiometer style), and Text Input (sensor value field); selecting an interaction type shows the relevant config fields (e.g. drag axis and value range for potentiometer style)
- [ ] **API functions** — a list of new `ArduinoAPI` entries this component needs, each with a function name, return type, and parameter list; optional preprocessor replacement string if the sketch-facing name differs from the `api->` name; optional injected header wrapper for overloads or default arguments
- [ ] **Preview** — before committing, shows a diff-style view of every file that will be created or modified; a single Generate button commits all changes at once

**What the generator writes:**
- [ ] `src/components/yourcomponent.cpp` — `ComponentItem` subclass with detection keywords, detection patterns, pin roles, and `paint()` stub; self-registers with `ComponentRegistry` at static init time
- [ ] Appends new function pointers to the end of `ArduinoAPI` struct in `arduinoapi.h`
- [ ] Appends `impl_*` static declarations to `arduinoruntime.h`
- [ ] Appends `impl_*` implementations to `arduinoruntime.cpp`
- [ ] Appends `get_api()` assignments in `arduinoruntime.cpp`
- [ ] Appends preprocessor replacement lines to `preprocessor.cpp`
- [ ] Appends injected header declarations or wrappers to `injected_header.inc` and flags that a cmake re-run is needed
- [ ] If an inject path is needed (sensor with a canvas input field), generates the full chain: `inject_xxx` in `ArduinoRuntime`, delegator in `SketchHost`, slot in `SketchThread`, signal declaration in `CanvasWidget`, and connection in `MainWindow`

**New simple components:**
- [ ] RGB LED — three PWM pins (R/G/B), detected from `#define` pin names; canvas shows a colored circle that blends the three channel values in real time
- [ ] Rotary encoder — two digital pins (CLK/DT) plus optional button pin; canvas shows a turn dial; pairs with `attachInterrupt` for accurate pulse counting

**New complex components:**
- [ ] Joystick — two analog axes (X/Y, 0–1023) plus a digital button; detected from `#define` pin names; canvas shows dual sliders and a clickable button
- [ ] Stepper motor — step count and direction tracked from STEP/DIR or IN1-IN4 pin patterns; canvas displays a position counter and rotation indicator
- [ ] Keypad matrix — 4x4 or 3x4, detected from defines, clickable grid on canvas
- [ ] DHT11 / DHT22 sensor — temperature and humidity; single-pin 1-wire timing protocol; `#include <DHT.h>` stripped and replaced with an injected class; constructor stores pin and sensor type; `dht.read()` returns the canvas-injected temperature/humidity values; canvas shows a temperature + humidity input widget; detected from `DHT_PIN` / `DHTPIN` keyword patterns or `DHT(` constructor call

**New display components:**
- [ ] 7-segment display — single and multi-digit, segment-accurate rendering
- [ ] MAX7219 LED matrix — `LedControl.h` injection (`.inc` file); `ComponentItem` renders an 8x8 grid of cells toggled by row/column data from `setLed` / `setRow` / `setColumn`; `setIntensity` and `shutdown` stubbed; CS/CLK/DIN pins detected via the multi-pin role map in the component definition
- [ ] Basic OLED — text and simple graphics (SSD1306-compatible); `Adafruit_SSD1306.h` or `U8g2` injection
- [ ] NeoPixel / WS2812B strip — individually addressable RGB LEDs, single-pin protocol, configurable strip length; `Adafruit_NeoPixel.h` injection

> **Milestone:** All existing components registered through the plugin system with three-tier detection; adding a new component requires filling out the generator form and clicking Generate with no manual file editing; RGB LED, rotary encoder, joystick, keypad, 7-segment, OLED, and NeoPixel sketches all run on the canvas.

---

### Phase 9 — Protocol Libraries + Low-level AVR Simulation

Heavier runtime work requiring more architectural changes: bus protocol simulation, virtual device responses, and direct register access.

**Protocol libraries (preprocessor injection + virtual device responses):**
- [ ] `Wire.begin` / `Wire.write` / `Wire.read` — byte-level I2C simulation; no electrical bus characteristics; device responses come from the virtual I2C device panel
- [ ] Virtual I2C device panel — "Devices" tab in the debug panel; table of 7-bit address → response byte sequence; when the sketch calls `Wire.requestFrom(addr, n)`, the runtime looks up the address and returns the configured bytes; entries are editable at runtime; covers the common pattern of reading a sensor register: `Wire.beginTransmission` → `Wire.write(reg)` → `Wire.endTransmission` → `Wire.requestFrom` → `Wire.read()`
- [ ] `SPI.begin` / `SPI.transfer` — same scope as Wire

**Low-level AVR simulation:**
- [ ] AVR GPIO register simulation — `DDRB`, `PORTB`, `PINB`, `DDRC`, `PORTC`, `PINC`, `DDRD`, `PORTD`, `PIND` etc. as overloaded-operator structs in injected header; reads/writes map to the same pin state as `digitalWrite`/`digitalRead`; bit-mask operations (`PORTB |= (1 << PB5)`) work correctly by routing through `impl_digitalWrite` per affected pin
- [ ] AVR hardware timer register simulation — `TCCR1A`, `TCCR1B`, `OCR1A`, `OCR1B`, `TIMSK1`, `TCNT1` etc. as overloaded-operator structs; writes to `OCR1A`/`OCR1B` update the corresponding pin's PWM duty cycle via the existing `analogWrite` path; `TIMSK1` overflow and compare-match interrupt enable bits register callbacks in `RuntimeState` that fire on the simulated timer tick; covers sketches that configure hardware PWM or use Timer1/Timer2 for precise timing without calling `analogWrite` directly; also completes `TIMER1_OVF_vect` / `TIMER2_OVF_vect` / `TIMER1_COMPA_vect` / `TIMER1_COMPB_vect` ISR dispatch from Phase 7

> **Milestone:** Sketches using I2C/SPI sensor libraries compile and run; direct GPIO register writes and hardware timer configuration work correctly.

---

### Phase 10 — Editor + Settings + Canvas Improvements

Polish the editor into a first-class coding environment, consolidate settings, add a serial plotter, and give the canvas a proper layout system.

**Editor:**
- [ ] **Code completion** — Ctrl+Space shows a filtered popup of Arduino API functions plus all functions, variables, and `#define` constants declared in the current sketch
- [ ] **Find & Replace** — Ctrl+F opens an inline find bar; Ctrl+H adds a replace field; Enter steps through matches, Escape dismisses
- [ ] **Save in-place** — Ctrl+S saves silently to the current file path when a sketch is already open; only prompts for a name on first save of a new unsaved sketch
- [ ] **Autosave / crash recovery** — editor content written to a `.autosave` file in the sketch folder every 30 seconds; on next open, if an `.autosave` file is newer than the `.cpp` file, offer to restore it; file is deleted on a clean save or close
- [ ] **Unsaved changes indicator** — append `*` to the window title when the editor content differs from the saved file; clear it on save
- [ ] **Auto-close brackets** — typing `(`, `[`, `{`, or `"` inserts the matching closer and positions the cursor inside; typing the closer when it is the next character skips over it instead of doubling
- [ ] **Bracket matching** — when the cursor sits adjacent to `(`, `)`, `{`, `}`, `[`, or `]`, highlight the matching bracket
- [ ] **Comment toggle** — Ctrl+/ adds `// ` to the current line or selected lines; pressing again removes it
- [ ] **Font size zoom** — Ctrl+`+` / Ctrl+`-` / Ctrl+scroll adjusts the editor font size; resets to default with Ctrl+`0`
- [ ] **Duplicate line** — Ctrl+D copies the current line and inserts it on the line below
- [ ] **Compile warnings** — compiler warnings surfaced in the editor alongside errors; yellow line backgrounds for warning lines with corrected line numbers
- [ ] **Sketch templates** — "New Sketch" dialog offers built-in starters (Blink, Button, Serial Echo, etc.); selected template copied into the new sketch folder
- [ ] **Example sketch library** — a browsable panel of complete working sketches organized by component type (LED, Servo, LCD, Distance Sensor, etc.); selecting one opens it as a new sketch ready to run
- [ ] **In-app Arduino API reference** — a collapsible panel or right-click lookup showing the signature, parameter descriptions, and return value for any Arduino function; covers all functions VEMCODE supports

**Serial Plotter:**
- [ ] Numeric values printed via `Serial.println()` graphed over time in a scrolling plot panel alongside the serial monitor; multiple named variables supported via `Serial.print("label:"); Serial.println(value);` format, matching the Arduino IDE Serial Plotter protocol

**Settings panel:**
- [ ] **Compiler path** — auto-detect common g++ install locations on first run (MinGW on Windows, `/usr/bin/g++` on Linux); show a validation indicator (green tick / red cross) next to the path field
- [ ] **Component configuration** — right-click any canvas component to open a config dialog for that component's parameters (NeoPixel strip length, 7-segment digit count, keypad matrix size, etc.); values saved to the `.vblayout` file alongside position
- [ ] **Syntax highlight colors** — let the user customize the editor color scheme; a small set of built-in themes (default dark, light, high contrast) plus manual overrides; saved to `settings.ini`
- [ ] **Canvas theme** — dark/light canvas background toggle; component colors adapt automatically
- [ ] **Auto-compile on save** — toggle in settings; when enabled, saving immediately triggers a compile without a manual Run click; separate keybind for run
- [ ] **Default sketch location** — configurable root folder for new sketches

**Canvas improvements:**
- [ ] Canvas layout mode — "Layout" toolbar button, components become draggable
- [ ] Positions saved to `sketch_name.vblayout` next to `.cpp` file; use saved positions on load, auto-generate otherwise
- [ ] Wire visualization improvements — color-coded by signal type (digital, analog, PWM)
- [ ] Canvas zoom — Ctrl+scroll or pinch to zoom in/out; zoom level saved per sketch in the `.vblayout` file

**Signal timeline protocol decoder:**
The signal timeline already records every `(timestamp_µs, pin, level)` transition — the same data a logic analyzer captures. Add a decoder layer that runs over this stream:
- [ ] Auto-threshold decoder — measures pulse widths, clusters them into two groups (short = 0, long = 1), and emits a decoded bitstream; handles any NRZ or pulse-width-encoded protocol without knowing the specific protocol in advance
- [ ] Decoded bytes panel — displays the recovered bitstream and byte values alongside the waveform
- [ ] Component routing — decoded bytes from a known pin are routed to that component's virtual RX buffer so it can respond
- [ ] Known protocol hints — `CircuitDetector` can annotate pins with the expected protocol (e.g. NeoPixel data pin → WS2812B decoder, DHT data pin → DHT decoder) so the right decoder is applied automatically

> **Milestone:** The editor feels complete for day-to-day sketch writing; serial plotter graphs live data; canvas layout can be saved and restored; bit-banged protocols are decoded in the signal timeline.

---

### Phase 11 — Component Visual Upgrades

Replace placeholder colored rectangles with proper component graphics. Visual work for this phase is being handled separately from the rest of the roadmap.

- [ ] Replace colored rectangles with QPainter-drawn component visuals for all existing components — animations (LED glow, buzzer pulse, motor rotation) driven by a shared canvas `QTimer` incrementing a `phase_` value on each active item
- [ ] Architecture is SVG-ready by design: swapping any component's visuals to `QSvgRenderer`-based rendering later only requires changing that component's `paint()` implementation
- [ ] LCD 16x2 — pixel-accurate character cell rendering, backlight color, cursor blink

> **Milestone:** Canvas looks polished with recognizable component graphics; adding SVG art for any component in the future is a single-file `paint()` swap.

---

### Phase 12 — Memory Analysis

Give the user realistic flash and RAM usage figures without requiring a real AVR toolchain on the hot path.

- [ ] `avr_gcc_path` setting in settings dialog; degrades gracefully if not configured — shows nothing rather than blocking Run
- [ ] After successful compile, run `avr-gcc` for size analysis only (not execution)
- [ ] Parse `avr-size` output for flash and SRAM usage
- [ ] Flash → hard enforce: block Run if over board's flash limit
- [ ] Static RAM → hard enforce: block Run if globals exceed board's SRAM limit
- [ ] Dynamic RAM (String/malloc) → warn but don't block; track via `malloc`/`new` interception
- [ ] Memory bar in UI: `████░░░░ 1234 / 32256 bytes (3%)`
- [ ] Warn at >75% usage before hitting the limit
- [ ] Auto-detect Arduino IDE `avr-gcc` path on first run
- [ ] Export `.hex` — toolbar button generates a flashable `.hex` for real hardware once avr-gcc is configured; output placed in the sketch subfolder alongside the compiled DLL

> **Milestone:** Flash and RAM usage shown for every compile; over-limit sketches are blocked from running; `.hex` can be exported for flashing to real hardware.

---

### Phase 13 — Multi-board Simulation

Run two boards simultaneously in the same session.

- [ ] Two `SketchThread` instances running at the same time
- [ ] Thread-safe state injection — replace `pin_values`, `analog_values`, and `pwm_values` arrays in `RuntimeState` with `std::atomic<int>` so UI inject calls and both sketch threads can't race on shared state
- [ ] Virtual serial pipe — TX of one board feeds RX of the other
- [ ] Both canvases visible simultaneously
- [ ] Enables master/slave, sensor node + controller, and I2C peripheral sketches

> **Milestone:** Two sketches communicate over virtual Serial and both canvases update correctly.

---

### Phase 14 — Step-through Debugger

Pause execution at any line and step statement by statement.

- [ ] Clickable gutter breakpoints in the editor — clicking a line number sets or clears a breakpoint marker
- [ ] Preprocessor injects `api->vb_breakpoint(line)` calls before each statement when debug mode is active
- [ ] `impl_vb_breakpoint` blocks the sketch thread on a condition variable when the line matches an active breakpoint — execution suspends, thread is paused but not stopped
- [ ] Step / Resume toolbar buttons — Step advances to the next `vb_breakpoint` call; Resume runs freely until the next breakpoint
- [ ] Variable watch panel updates live while paused — shows current state of all watched variables at the breakpoint line
- [ ] Canvas state reflects the paused moment — pin states, component positions, and signal timeline are all frozen at the breakpoint

> **Milestone:** A sketch can be paused at any line, stepped through statement by statement, and variable values inspected at each step.

---

> ### Beta Release
> Tag `v1.0-beta` after Phase 12 is complete. The tool is feature-complete enough for real-world use: the editor is polished, the API surface covers the common cases, and users get meaningful memory feedback. Installer and auto-update (QtIFW + GitHub Releases as update repository) ships with the stable `v1.0` release — not before.

---

### Later

- Installer — QtIFW with GitHub Releases as the update repository; bundle MinGW for zero-dependency install on Windows; package for common Linux distros
- macOS support
- Additional board profiles (STM32, etc.) — add one `BoardProfile` entry each
- Hardware Bridge — run a sketch inside VEMCODE while reading from physical sensors and driving physical hardware over USB serial; no code changes between virtual and hybrid operation
- MicroPython / CircuitPython support — Python execution path on Pico and compatible boards using the same runtime, canvas, and signal timeline
- ESP32 + Network Simulation — WiFi stubs and a mock HTTP server; deterministic, offline, and repeatable responses for firmware testing
- Automated regression test suite — a set of reference sketches with known expected serial output; compile + run each sketch headlessly (`QT_QPA_PLATFORM=offscreen`), capture serial output, assert it matches; run as a CI step to catch preprocessor or runtime regressions before they ship
