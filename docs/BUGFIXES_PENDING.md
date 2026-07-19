# Pending Bug Fixes — Full Codebase Review (2026-07-17)

Full read-through of `src/` (~9,500 lines) plus the injected library headers in
`src/core/build/libs/`, done in six parallel passes: build pipeline, circuit
detection, host/runtime, `mainwindow.cpp`, remaining UI widgets, and all 18
component files. This file is the punch list. Ranked roughly by severity.

All items originally listed under "Fix first" have been fixed (cross-thread
ISR dispatch/mutual exclusion, `normalize_call_whitespace()` regression, bare
H-bridge pin grouping, LCD `setCursor` bounds, autosave-on-close, silent file
open/write failures, the stuck `@board` hint Run button, and `tone()`'s
unjoined thread).

## Worth fixing soon (medium)

- `src/core/circuit/circuitdetector.cpp:316-321` — leftover pin-role
  assignment can mis-wire a motor pin into the wrong role just because of map
  ordering.
- `src/components/color_sensor.cpp:90` — registered as `Array` grouping
  strategy, but real TCS230/3200 sketches use separate `#define`s, not an
  array — detection path is effectively dead for real sketches.
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
- Earlier watchdog-thread/dlopen-race fixes are still structurally intact,
  and the timer/ISR-dispatch threading bug that had reintroduced a related
  issue is now fixed too.
- `linenumberarea.cpp` is intentionally an (almost) empty translation unit —
  needed for AUTOMOC to generate moc code for the header-only `Q_OBJECT`
  classes in `linenumberarea.h`. Not dead code.
