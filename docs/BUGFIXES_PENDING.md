# Pending Bug Fixes — Full Codebase Review (2026-07-17)

Full read-through of `src/` (~9,500 lines) plus the injected library headers in
`src/core/build/libs/`, done in six parallel passes: build pipeline, circuit
detection, host/runtime, `mainwindow.cpp`, remaining UI widgets, and all 18
component files. This file is the punch list. Ranked roughly by severity.

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
- `src/ui/editor/codehighlighter.cpp:110` — comment/string regexes don't
  respect each other's context; `"http://example.com"` gets partially
  recolored as a comment.
- `src/ui/mainwindow.cpp`'s `eventFilter` still has the editor-only key
  handling (tab-insert, auto-indent, bracket auto-close, dedent,
  duplicate-line, comment-toggle/completion dispatch) that belongs on
  `EditorWithLines` instead — deferred out of the `SketchLinter`/
  `KeybindManager`/`FindReplaceBar` extraction since it needs callbacks back
  into `MainWindow` and is the most coupled of the four.
