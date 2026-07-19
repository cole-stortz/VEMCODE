# Pending Bug Fixes — Full Codebase Review (2026-07-17)

Full read-through of `src/` (~9,500 lines) plus the injected library headers in
`src/core/build/libs/`, done in six parallel passes: build pipeline, circuit
detection, host/runtime, `mainwindow.cpp`, remaining UI widgets, and all 18
component files. This file is the punch list. Ranked roughly by severity.

## Fix first (real, verifiable bugs)

### 1. Cross-thread ISR dispatch is broken
`src/core/runtime/arduinoruntime.cpp:10`

`g_runtime` is a `thread_local` pointer, set on the sketch thread and the GUI
thread, but never set inside `timer_thread_` (arduinoruntime.cpp:847-862) or
`wdt_thread_` (arduinoruntime.cpp:553-605), both of which call sketch ISR
handlers directly. Any `TIMER1_COMPA_vect`/`TIMER2_*_vect`/`WDT_vect` handler
that calls `digitalWrite`/`millis`/etc. silently no-ops, since every `impl_*`
checks `if (!g_runtime) return;`.

On top of that, there's no mutual exclusion between ISR handlers firing on
different threads and `loop()` — real AVR ISRs never run concurrently, this
can. And crash recovery (`sigsetjmp` in `sketchhostthread.cpp:6-14`) only
guards the sketch thread, so a crash inside a timer/watchdog ISR handler
takes down the whole process instead of surfacing `sketchCrashed`.

This is the highest-impact finding — any hardware-timer or watchdog ISR that
touches the Arduino API is silently broken right now. It's new code sitting
on top of the (still-intact) previously-fixed watchdog/dlopen bugs.

### 2. `normalize_call_whitespace()` appears to have regressed
`src/core/build/preprocessor.cpp:45`

Project memory documents adding `normalize_call_whitespace()` to fix
`Serial.begin (9600)` (space before paren) — grepped the current file and
full git history, the function doesn't exist anywhere anymore.
`replace_api_calls` now requires the call name immediately followed by `(`,
so that class of sketch silently fails to transform and hits a compile
error. Check whether this got dropped during a refactor.

### 3. Bare H-bridge pin naming never groups into one component
`src/core/circuit/circuitdetector.cpp:295-297`

`detect_prefix_group` requires an underscore in the `#define` name to derive
a group key. The extremely common `#define ENA 5 / IN1 6 / IN2 7` naming (no
shared prefix) never groups — three separate single-pin boxes instead of one
`HBridgeMotor`/`Stepper`, losing PWM/direction semantics. Likely the
most-hit real-world bug given how common that naming convention is.

### 4. LCD `setCursor` allows out-of-bounds writes
`src/core/build/libs/liquidcrystal.inc:14`

Clamps upper bounds only; `lcd.setCursor(-1, 0)` writes to
`buf_[row_][-1]` — memory corruption/crash risk in the host process, not
just a display glitch.

### 5. Autosave gets deleted even with unsaved changes
`src/ui/mainwindow.cpp:286-292`

`closeEvent` deletes the `.autosave` file unconditionally, without checking
`document()->isModified()` or prompting. This defeats the crash-recovery
feature — close with unsaved edits and both the in-editor changes and the
recovery safety net are gone.

### 6. Silent failures on file open / compile
`src/ui/mainwindow.cpp:1015-1037` (open), `src/ui/mainwindow.cpp:828-832` (compile write)

`onOpenClicked` updates the path/title even if `file.open()` fails — editor
keeps stale content but now points at the new path, so the next save
overwrites the new file with old text. Similarly, writing the sketch before
compiling doesn't check the write succeeded.

### 7. Bad `@board` hint permanently disables Run
`src/ui/mainwindow.cpp:857-869`

Returns early after disabling the Run button, without ever re-enabling it or
clearing the "Compiling..." status — stuck until restart.

### 8. `tone()`'s helper thread isn't tracked/joined
`src/core/runtime/arduinoruntime.cpp:389-409`

Unlike `wdt_thread_`/`timer_thread_` (hardened by earlier fixes), this one is
`.detach()`'d with only a racy `stop_requested_` check — it can still write
into `state_` during/after `reset_state()`'s destroy/reconstruct.

## Worth fixing soon (medium)

- `src/core/circuit/circuitdetector.cpp:316-321` — leftover pin-role
  assignment can mis-wire a motor pin into the wrong role just because of map
  ordering.
- `src/components/color_sensor.cpp:90` — registered as `Array` grouping
  strategy, but real TCS230/3200 sketches use separate `#define`s, not an
  array — detection path is effectively dead for real sketches.
- `src/core/circuit/circuitdetector.cpp:601-618` — `#define` chain
  resolution has no cycle guard; a self-referential typo stack-overflows the
  app while parsing.
- `src/core/build/preprocessor.cpp` — `replace_all` (digitalWrite/pinMode/
  delay/etc.) lacks the word-boundary guard `replace_token` has, and runs
  over string literals/comments unconditionally. A user function named
  `mydelay(` gets corrupted; a debug string containing `"delay(1000)"` gets
  silently rewritten.
- `src/core/build/preprocessor.cpp:266-280` `inject_safety_delay` assumes
  `loop()` is the last function in the file (uses `rfind('}')` over the
  whole file) — if helper functions are defined after `loop()`, the
  anti-freeze delay lands in the wrong place and a delay-free `loop()` still
  spins unthrottled.
- `src/ui/codehighlighter.cpp:110` — comment/string regexes don't respect
  each other's context; `"http://example.com"` gets partially recolored as a
  comment.
- `src/ui/mainwindow.cpp:474` — find/replace match offsets go stale if you
  edit the document while the find bar is open.
- `src/components/rgb_led.cpp:37-42` — no clamping on pin values before
  building a `QColor`, unlike `color_sensor.cpp`'s `qBound` calls for the
  same kind of data.
- `src/components/stepper.cpp:6,27` — `STEPPER_INACTIVE` is declared but
  never used; the component always paints as "active."

## Structural observations

- **`mainwindow.cpp` (2053 lines) is a God object** — editor logic, a
  200+ line static linter, autocomplete, find/replace, keybinds, autosave,
  theme application all live in one class. Candidates to extract: a
  `SketchLinter`/error-formatter (already Qt-independent), a
  `FindReplaceBar`, a `KeybindManager`, and pushing editor-only key handling
  into `EditorWithLines` instead of `MainWindow::eventFilter`.
- **`arduinoruntime.cpp`/`.h` (868/283 lines)** covers GPIO, 4 serial
  variants, EEPROM, tone, interrupts, watchdog, sleep, Wire, SPI, and timers
  via ~60 static functions — a natural split point per peripheral.
- **~150 lines of duplicated paint boilerplate** across all 19 component
  files (border/luminance/font/text-draw) with no shared helper on the
  common `ComponentItem` base — `analog_sensor.cpp` already solved this with
  a shared base class; `button.cpp`'s near-identical `ButtonItem`/
  `ButtonCleanItem` didn't adopt that pattern.
- Board-name → `BoardProfile` mapping duplicated 4x across
  `mainwindow.cpp`/`main.cpp`. `parseByteList` duplicated verbatim between
  `spipanel.cpp` and `devicespanel.cpp`. `QSettings(...+"/settings.ini")`
  reconstructed at 15+ call sites instead of one accessor.
- `apptheme.cpp`'s `AppPalette` is 21 unnamed positional `QColor` fields
  cross-referenced against a separate `%1..%21` stylesheet template —
  correct today, fragile to future reordering.

## What's actually solid (confirmed clean, no action needed)

- **Signal-before-connect ordering** (the documented Qt gotcha where a
  component pushes its initial value before anything's connected to receive
  it) is clean across the board — verified in `canvaswidget.cpp`'s
  `placeComponent` and in all 19 component files, including the two newest
  (`stepper.cpp`, `seven_segment.cpp`).
- Multi-pin detection strategies (`detect_suffix_group`/`detect_prefix_group`/
  `detect_array_group`/`detect_singleton_group`) are already split into small
  dedicated functions rather than one dispatch blob.
- Earlier watchdog-thread/dlopen-race fixes are still structurally intact —
  it's specifically the new timer/ISR-dispatch code (see #1 above) that
  reintroduces a related class of threading bug.
- `linenumberarea.cpp` is intentionally an (almost) empty translation unit —
  needed for AUTOMOC to generate moc code for the header-only `Q_OBJECT`
  classes in `linenumberarea.h`. Not dead code.
