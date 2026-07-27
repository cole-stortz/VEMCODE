# VEMCODE : Architecture

**Overview:**
This document covers a more in depth viewpoint about how VEMCODE actually works in the back-end. If you want to learn about how the current runtime executes the functions at a base level using the pointer table, understand how the configuration of the UI is developed and connected to the runtime, and more intricate details that is explained here.

## Table of Contents
- [VEMCODE : Architecture](#vemcode--architecture)
  - [Table of Contents](#table-of-contents)
  - [System Architecture](#system-architecture)
  - [Core Components](#core-components)
    - [Preprocessor](#preprocessor)
    - [Compiler](#compiler)
    - [Sketch Host](#sketch-host)
    - [Sketch Thread](#sketch-thread)
    - [Arduino Runtime](#arduino-runtime)
      - [RuntimeState](#runtimestate)
      - [Function Pointer Table](#function-pointer-table)
      - [Digital I/O](#digital-io)
      - [Analog I/O](#analog-io)
      - [Timing](#timing)
      - [Serial](#serial)
      - [Interrupts](#interrupts)
      - [EEPROM](#eeprom)
      - [Watchdog Timer](#watchdog-timer)
      - [Sleep Modes](#sleep-modes)
      - [Misc](#misc)
      - [Wire / I2C](#wire--i2c)
      - [SPI](#spi)
      - [AVR GPIO Registers](#avr-gpio-registers)
      - [AVR Timer Registers](#avr-timer-registers)
    - [Circuit Detector](#circuit-detector)
    - [Error UX](#error-ux)
    - [Library Injection Files](#library-injection-files)
  - [UI Components](#ui-components)
    - [Main Window](#main-window)
      - [Editor panel](#editor-panel)
      - [Canvas Panel](#canvas-panel)
      - [Debug Panel](#debug-panel)
      - [Toolbar](#toolbar)
    - [Canvas Widget](#canvas-widget)
      - [Auto Layout](#auto-layout)
      - [Component Rendering](#component-rendering)
      - [Input Handling](#input-handling)
  - [Data Flow](#data-flow)
  - [Board Profiles](#board-profiles)
  - [Adding Components](#adding-components)
  - [Headless Mode](#headless-mode)
  - [Building \& Testing](#building--testing)
    - [Build Scripts](#build-scripts)
    - [Test Suite](#test-suite)
    - [Windows Cross-Build](#windows-cross-build)

## System Architecture
```
Your sketch (.cpp)
    → Preprocessor — transforms Arduino syntax to shared library format
    → g++ — compiles to .so (Linux) or .dll (Windows)
    → SketchHost — loads the library, extracts vb_init/vb_setup/vb_loop
    → SketchThread — runs vb_loop on a background thread
    → Runtime — implements all API calls, fires UI callbacks
    → UI — canvas, serial monitor, signal timeline, variable watch
```
## Core Components

### Preprocessor
How it transforms sketch syntax to shared library format.
- Skips the whole pipeline if the source already looks transformed (contains `vb_init`/`ArduinoAPI`).
	- So re-processing a temp file never double-transforms it.
- Transform ISR blocks and assembly code:
	- EX: `ISR(vect_name)` >> `void __vb_isr_X_vect() { body }`.
	- EX: `cli` >> `noInterrupts()`.
- Replace functions with api call:
	- EX: `digitalRead(` >> `api->digitalRead(`.
- Strip includes and replace them if needed:
	- EX: `#include <avr/io>` >> `null`.
	- EX: `#include <Servo>` >> injected `g_servo_lib` content.
		- Same mechanism covers `Wire`, `SPI`, `LiquidCrystal`, `SoftwareSerial`, `Keypad`, `DHT`, `LedControl` (see [Library Injection Files](#library-injection-files)).
- Generate forward declarations like Arduino IDE does automatically.
- Wrap setup and loop with DLL exports.
- Inject register_isr() calls into setup() for each found ISR vector.
- Inject a safety delay to prevent freezes.
	- One before `vb_loop()`'s closing brace if the sketch never calls `delay()` anywhere.
	- A separate scan walks every `while` loop body and injects `api->delay(1);` into any one missing its own `delay()`/`delayMicroseconds()`, so a busy-wait `while` loop that never blocks elsewhere still yields.
- Inject custom header to cover string class, math functions, and other default functions needed.
- Collects warnings along the way (unrecognized `#include`s, ISR vectors with no simulated equivalent, unsupported asm instructions).
	- Surfaced via `takeWarnings()` rather than failing the compile.

### Compiler
How g++ is invoked, flags, output location, error capture.
- Create the sketch path with appropriate library format.
	- EX: .so for linux, .dll for windows, .dylib for macOS.
- Pre check if setup and loop are included before wasting compile time.
- Extract the board name hint from the original source before preprocessing.
	- EX: `// @board <name> `
- Run the preprocessor.
- Write the preprocessed output to a fixed temp file for hot reloading.
	- Not sketch-specific, so two sketches compiling into the same output directory would collide.
	- EX: `_vb_temp.cpp`
- Collect any extra .cpp files in the sketch folder to add them to the compile command.
- On Windows, additionally links `-static-libgcc -static-libstdc++` and passes `-Wl,--export-all-symbols`.
	- Needed so Variable Watch's dlsym-by-name polling can still find sketch globals; MinGW doesn't export DLL symbols by default the way a Linux `.so` does.
- Write the command to the ostringstream to run the compile.
- Read the results and parse errors if there are any then return.
### Sketch Host
dlopen/LoadLibrary, temp copy strategy, hot-reload mechanism.
- First free any previous loaded libraries.
- Copy to a PID-scoped temp file for hot reloading, not just a fixed `.tmp` name.
	- So two VEMCODE processes running the same sketch never overwrite each other's mapped file; doing so while it's still dlopen'd elsewhere causes a SIGBUS.
	- EX: `blink.so.5678.tmp.so` or `blink.dll.5678.tmp.dll`
- Open using dlopen on linux or LoadLibraryA on windows.
- Extract vb_init/setup/loop, dlsym on linux and GetProcAdress on windows.
- `needs_reload()`/`reload_if_changed()` compare the source file's mtime to detect hot-reload.
- `~SketchHost()` stops the runtime's background threads before unloading the library.
	- They call function pointers into it, so they have to stop first.
	- Then deletes its own PID-scoped temp file.
- `read_watched_variable()` backs the Variable Watch panel: dlsym's an arbitrary sketch global by name.
	- Interprets the raw pointer per a `WatchVarType` (Int/Float/Long/ULong/Bool) the caller supplies, since dlsym only ever returns `void*`.
- Hosts connections to runtime and runs the loop.
	- Plus a growing set of `inject_*` passthroughs (analog, serial, pulse duration, color sensor, keypad, DHT, Wire, SPI) that all forward straight to the matching `ArduinoRuntime::inject_*`.
	- EX: `SketchHost::inject_pin` >> `runtime_.inject_pin`
### Sketch Thread
Background thread, loop execution model, stop mechanism.
- Wires every runtime callback (`on_serial_output`, `on_serial1/2_output`, `on_soft_serial_output`, `on_pin_changed`, `on_variable_changed`, `on_lcd_print`, `on_matrix_row`, `on_watchdog_reset`, `on_sleep_changed`) to a matching Qt signal.
	- Replaces the runtime's stdout fallback.
- Loads the sketch DLL under `exec_mutex()`; `setup()` runs here, on this same thread, before the loop below ever takes the lock itself.
	- The preprocessor always renames the sketch's `setup()` to `vb_setup()`, there's no literal `void setup()` call path.
	- The lock has to already be held during `load()` because `setup()` can call `delay()`, which unconditionally unlocks/relocks `exec_mtx_` to let ISRs preempt it. An unlock from a thread that doesn't hold the mutex is undefined behavior.
- The main loop takes `exec_mutex()`, calls `host_.run_loop()`, releases it, and catches any exception as a sketch crash (`sketchCrashed` signal).
	- A matching pair of OS-level `SIGFPE`/`SIGSEGV` handlers (via `sigaction`/`sigsetjmp` on POSIX, `signal`/`setjmp` on Windows) additionally recovers from hard crashes a C++ `try`/`catch` can't catch, like a null-pointer write inside the sketch.
- Every iteration also checks, on independent timers, not every loop, whether the sketch file changed on disk and whether any registered Variable Watch entries need re-polling.
	- File change: 500ms, `reload_if_changed()` → `sketchReloaded` signal.
	- Variable watch: 100ms, `read_watched_variable()` per watched name → `variableChanged` signal.
	- Polled from this same thread specifically so a read of the sketch's own globals never races the sketch's own writes to them.

### Arduino Runtime

#### RuntimeState
The `RuntimeState` struct holds all simulation states for a running sketch. One instance lives inside `ArduinoRuntime` and is accessed through the `g_runtime` thread-local pointer.
```c++
struct RuntimeState {
    int  pin_modes[80]    = {};
    int  pin_values[80]   = {};
    int  analog_values[20] = {};
    int  pwm_values[80]    = {};
    unsigned long pulse_durations_[80] = {};
    bool pin_driven[80] = {};  // true once a UI component has injected this pin
    int  pin_bounce_target[80] = {};
    std::map<int, std::chrono::steady_clock::time_point> pin_bounce_until_;
    std::chrono::steady_clock::time_point start_time;
    bool serial_started = false;
    int  serial_baud    = 0;
    std::array<uint8_t, 1024> eeprom_;
    std::map<int, void(*)()> interrupt_callbacks_; // pin → callback
    std::map<int, int>       interrupt_modes_;     // pin → mode
    std::map<std::string, void(*)()> isr_handlers_; // vector name → handler
    bool interrupts_enabled_ = true;
    std::map<int, std::deque<char>> soft_serial_buffers_; // rxPin → RX buffer
    std::map<int, std::array<unsigned long, 4>> color_channels_;
    std::map<int, int> tone_frequencies_;

    // Watchdog/Sleep share one mutex/condvar, a watchdog timeout is one of the
    // things that has to be able to wake a sleeping sketch
    bool wdt_enabled_ = false, sleep_enabled_ = false, sleep_woken_ = false;
    std::condition_variable sleep_cv_;
    std::mutex sleep_mtx_;
    std::mutex exec_mtx_; // serializes loop() against ISR handlers

    // Wire/I2C, SPI: each guarded by its own mutex, written from the GUI
    // thread (inject_wire_device/inject_spi_bytes), read from the sketch thread
    std::map<int, std::vector<uint8_t>> wire_devices_; // address → response bytes
    std::mutex wire_mtx_;
    std::vector<uint8_t> spi_response_bytes_; // cycled one byte per transfer()
    std::mutex spi_mtx_;

    // AVR hardware timers (Timer1 16-bit, Timer2 8-bit)
    std::mutex timer_mtx_;
    AvrTimerState timer1_{65536, 9, 10}, timer2_{256, 11, 3};
};
```
#### Function Pointer Table
`get_api()` links every Arduino API function to its `impl_*` static method and returns the struct to the sketch during load. The sketch calls functions through this table so all calls route back into the host runtime.
```c++
ArduinoAPI ArduinoRuntime::get_api() {
	g_runtime = this;
	state_.start_time = std::chrono::steady_clock::now();
	ArduinoAPI api;
	api.pinMode = impl_pinMode;
	api.digitalWrite = impl_digitalWrite;
	// Rest of the api functions...
	return api;
}
```
Every `impl_*` function starts with if(!g_runtime) return; in some way to check if the runtime is active, if not the call is a no-op.
#### Digital I/O
`digitalWrite` is the most active function in the runtime. It updates pin state, fires the `on_pin_changed` callback to update the canvas and signal timeline, then dispatches any registered interrupt handlers if the transition matches their mode.
```c++
void ArduinoRuntime::impl_digitalWrite(int pin, int value) {
    // 1. Bounds check, then update pin state, bail if unchanged
    if (pin < 0 || pin >= g_runtime->profile_.pin_count) return;
    int old_value = g_runtime->state_.pin_values[pin];
    g_runtime->state_.pin_values[pin] = value;
    if (old_value == value) return;

    // 2. Fire canvas/timeline callback, or stdout fallback in headless mode
    if (g_runtime->on_pin_changed)
        g_runtime->on_pin_changed(pin, value);
    else
        std::cout << "pin " << pin << " -> " << (value ? "HIGH" : "LOW") << "\n";

    if (!g_runtime->state_.interrupts_enabled_) return;

    // 3. Dispatch attachInterrupt callbacks
    auto cb_it = g_runtime->state_.interrupt_callbacks_.find(pin);
    if (cb_it != g_runtime->state_.interrupt_callbacks_.end()) {
        int mode = g_runtime->state_.interrupt_modes_[pin];
        bool fire = (mode == vb::CHANGE) ||
                    (mode == vb::RISING  && old_value == 0 && value == 1) ||
                    (mode == vb::FALLING && old_value == 1 && value == 0);
        if (fire) { /* disable interrupts, call cb, re-enable */ }
    }

    // 4. Dispatch ISR vector handlers
    if (pin == 2) dispatch_vec("INT0_vect");
    if (pin == 3) dispatch_vec("INT1_vect");
    if (pin >= 0  && pin <= 7)  dispatch_vec("PCINT2_vect");
    if (pin >= 8  && pin <= 13) dispatch_vec("PCINT0_vect");
    if (pin >= 14 && pin <= 19) dispatch_vec("PCINT1_vect");

    // 5. Wake a sleeping sketch -- any pin change counts as an interrupt source
    if (g_runtime->state_.sleep_enabled_) {
        std::lock_guard<std::mutex> lock(g_runtime->state_.sleep_mtx_);
        g_runtime->state_.sleep_woken_ = true;
        g_runtime->state_.sleep_cv_.notify_all();
    }
}
```
`digitalRead` handles two special cases before returning the pin value; Button bounce (returns random values during the bounce window then settles) and Floating pins (pins called in `digitalRead` that are unbound return random values to simulate noise).
#### Analog I/O
`analogRead` maps the pin number to an index in `analog_values[]` using the  `analog_offset` from the board profile (EX: Uno = A0 is 14 so `analog_index = pin - 14`). Optional gaussian noise can be enabled in settings to simulate real sensor noise.
```c++
int analog_index = (pin >= profile_.analog_offset) 
    ? pin - profile_.analog_offset 
    : pin;
// optional noise: normal distribution σ=2, clamped to 0-1023
```
`analogWrite` updates `pwm_values[]` and fires `on_pin_changed` so the signal timeline tracks PWM outputs.
#### Timing
All timing functions scale by speed_multiplier_ which is set from the speed slider (`1/speed`). `delay()` sleeps in 10ms chunks so stop requests are handled faster than waiting for the sketches full blocking duration, and releases `exec_mtx_` for each chunk so an ISR (attachInterrupt, a hardware timer, or the watchdog) can still preempt a delay() call, matching how a real interrupt fires during a busy-wait.
```c++
void ArduinoRuntime::impl_delay(unsigned long ms) {
    unsigned long scaled = (unsigned long)(ms * g_runtime->speed_multiplier_);
    unsigned long elapsed = 0;
    while (elapsed < scaled) {
        if (g_runtime->stop_requested_) return;
        unsigned long chunk = qMin(10UL, scaled - elapsed);
        g_runtime->state_.exec_mtx_.unlock(); // let ISR handlers preempt this chunk
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        g_runtime->state_.exec_mtx_.lock();
        elapsed += chunk;
    }
}

unsigned long ArduinoRuntime::impl_millis() {
    auto real_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - g_runtime->state_.start_time).count();
    return (unsigned long)(real_us / g_runtime->speed_multiplier_ / 1000);
}
```
`millis()` and `micros()` divide wall-clock time by speed_multiplier_ so sketch timing matches the slider setting.
#### Serial
All serial output functions check for an `on_serial_output` callback first. If connected, output goes to UI serial monitor, otherwise it falls back to stdout for headless testing.
```c++
void impl_Serial_print(const char* s) {
    if (g_runtime && g_runtime->on_serial_output)
        g_runtime->on_serial_output(std::string(s)); // → UI
    else
        std::cout << s; // → stdout fallback
}
```
`Serial1/Serial2` follows the same pattern with separate callbacks. `SoftwareSerial` routes outputs prefixed with `[SW:N]` to the main serial monitor, keyed by the RX pin.
#### Interrupts
Two separate interrupt systems exist in the runtime:
- `attachInterrupt()` stores a callback and mode keyed by pin number in `interrupt_callbacks_` and `interrupt_modes_` (`RISING`, `FALLING`, `CHANGE`).
- `register_isr()` stores handlers keyed by vector name string in `isr_handlers_` (i.e. `"INT0_vect"`, `"PCINT0_vect"`). Both systems temporarily set `interrupts_enabled_ = false` during dispatch to match AVR behavior.
Both Interrupt systems are dispatched from `impl_digitalWrite` when the relevant pin changes state.
#### EEPROM
A 1024 byte `std::array<uint8_t, 1024>` in `RuntimeState`.  During all access, the bounds are checked, out of range reads return `0xFF` matching real AVR behavior. `update()` skips the write if the value is unchanged. State does not persist between sessions.
#### Watchdog Timer
`wdt_enable()` starts a background monitor thread that polls whether `wdt_reset()` has been called recently enough; if not, it either fires the `WDT_vect` ISR (if the sketch registered one) or calls `on_watchdog_reset` to signal a hard reset. The WDT reuses the Sleep Modes mutex/condvar (`sleep_mtx_`/`sleep_cv_`) rather than owning a dedicated one, since a watchdog timeout is one of the things that has to be able to wake a sleeping sketch.

- `wdt_reset()`/`wdt_disable()` just update `wdt_last_reset_`/`wdt_enabled_` under `sleep_mtx_`.
- `wdt_enable()` calls `stop_wdt_thread()` first to join any previously running watchdog thread.
	- This has to happen before `state_`'s mutex/condvar are touched again, since an unjoined `std::thread` calls `std::terminate()` on destruction.
	- The thread closure captures `rt` (the runtime pointer) directly since `g_runtime` is `thread_local` and wouldn't be visible from the monitor thread.
- The monitor thread polls every 10ms.
	- On timeout, if the sketch is currently asleep it notifies the sleep condvar to wake it, then checks for a registered `WDT_vect` handler.
	- If registered: locks `exec_mtx_`, disables interrupts, fires the ISR, re-enables interrupts, unlocks, and resets `wdt_last_reset_`. The thread keeps looping, so this is a recoverable interrupt, not a reset.
	- If not registered: calls `on_watchdog_reset` and the thread exits, this is a hard reset. No further watchdog activity happens until the sketch calls `wdt_enable()` again.
- Locking order never holds `sleep_mtx_` and `exec_mtx_` at once.
	- The thread takes `sleep_mtx_` (briefly, to read/notify), then separately takes `exec_mtx_` for the ISR call.
	- This is the mirror image of `sleep_cpu()`'s own ordering (see below), and matters for avoiding a lock-order inversion between the two.
```c++
void ArduinoRuntime::impl_wdt_enable(int timeout_ms) {
    ArduinoRuntime* rt = g_runtime;
    rt->stop_wdt_thread(); // join previous monitor before touching state_ again
    { std::lock_guard<std::mutex> lock(rt->state_.sleep_mtx_);
      rt->state_.wdt_enabled_ = true;
      rt->state_.wdt_timeout_ms_ = timeout_ms;
      rt->state_.wdt_last_reset_ = std::chrono::steady_clock::now(); }

    rt->wdt_thread_ = std::thread([rt]() {
        while (!rt->wdt_thread_stop_ && !rt->stop_requested_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            // ... check age against wdt_timeout_ms_ under sleep_mtx_ ...
            auto isr_it = rt->state_.isr_handlers_.find("WDT_vect");
            if (isr_it != rt->state_.isr_handlers_.end() && isr_it->second) {
                rt->state_.exec_mtx_.lock();
                isr_it->second(); // fire under exec_mtx_, same as any other ISR
                rt->state_.exec_mtx_.unlock();
            } else {
                if (rt->on_watchdog_reset) rt->on_watchdog_reset();
                return;
            }
        }
    });
}
```
#### Sleep Modes
`sleep_cpu()` blocks the sketch thread on a condition variable until something wakes it: a digital pin transition, an ISR, or a watchdog timeout, matching real AVR sleep semantics where any enabled interrupt source can wake the chip.

- `sleep_enable()`/`sleep_disable()` just toggle `sleep_enabled_`; `sleep_cpu()` is a no-op unless sleep is enabled.
- Critically, `sleep_cpu()` unlocks `exec_mtx_` before waiting and re-locks it after waking.
	- Without that release, the watchdog thread (or `impl_digitalWrite`'s own wake-on-interrupt path, see below) could never acquire `exec_mtx_` to actually fire the interrupt that's supposed to wake the sketch up, deadlocking it asleep forever.
- The wake condition is `sleep_woken_ || stop_requested_`, so a stop request during sleep also unblocks it cleanly rather than hanging the shutdown path.
- Wakeup itself is triggered from two places: the watchdog thread on timeout, and `impl_digitalWrite()`.
	- Any pin change notifies the sleep condvar if `sleep_enabled_` is set, regardless of whether that pin has an attached interrupt, mirroring how any enabled interrupt source wakes real AVR sleep modes.
```c++
void ArduinoRuntime::impl_sleep_cpu() {
    if (!g_runtime->state_.sleep_enabled_) return;
    if (g_runtime->on_sleep_changed) g_runtime->on_sleep_changed(true);

    g_runtime->state_.exec_mtx_.unlock(); // let the waking thread acquire exec_mtx_
    { std::unique_lock<std::mutex> lock(g_runtime->state_.sleep_mtx_);
      g_runtime->state_.sleep_woken_ = false;
      g_runtime->state_.sleep_cv_.wait(lock, [] {
          return g_runtime->state_.sleep_woken_ || g_runtime->stop_requested_.load();
      }); }
    g_runtime->state_.exec_mtx_.lock();

    if (g_runtime->on_sleep_changed) g_runtime->on_sleep_changed(false);
}
```
#### Misc
`tone()` stores the frequency in `tone_frequencies_` and fires `on_pin_changed` so the canvas can show the buzzer active. If a duration is specified it spins up a detached thread that sleeps for the scaled duration then clears the tone.

`pulseIn()` has three paths:
- If a pulse duration was injected from the UI, it returns immediately.
- If the pin belongs to a color sensor, it reads from the color channel map.
- Otherwise it falls back to a three-phase polling loop.
	- Wait for idle >> wait for pulse start >> measure pulse end.

`watch_variable()` fires `on_variable_changed` which routes to the Variable Watch panel, or falls back to stdout.
#### Wire / I2C
Wire's optional arguments and overloaded `write()` don't fit a bare function-pointer signature, so these are wrapped as inline functions in the injected header instead of routed directly through `ArduinoAPI`, unlike most other calls.

- `write()` has overloads for a single byte, a C-string, and a `(buf, len)` pair, all funneling down to `api->wire_write()` one byte at a time.
- An extra set of overloads for `int`/`unsigned int`/`long`/`unsigned long` exists purely to break an ambiguity.
	- A literal like `Wire.write(0x00)` converts equally well to `uint8_t` or to a null `const char*`, so an exact-match overload is needed to win resolution, the same fix real Arduino's `Print::write()` uses.
- `endTransmission()` and `requestFrom()` both accept and discard the optional `stopBit` argument, forwarding to `api->wire_end_transmission()` / `api->wire_request_from(address, quantity)`.
	- No bus errors are modeled; `endTransmission()` always reports success once a runtime exists.
- Backed by the virtual I2C device panel (`devicespanel.cpp`): the panel's configured address and response bytes are what `requestFrom()` actually returns.
	- `pushAll()` re-emits every configured device on reload, since a sketch reload wipes `RuntimeState::wire_devices_`.

#### SPI
SPI has no per-device addressing in the API layer; real SPI selects a device via a plain `digitalWrite()` on a CS pin, so that's left entirely to the sketch, same as any other digital pin.

- `SPISettings`'s constructor accepts clock/bit-order/data-mode and discards all three; `MSBFIRST`, `LSBFIRST`, and `SPI_MODE0..3` are defined as no-op constants purely so real sketches compile unmodified.
- `transfer(uint8_t)` returns one byte from `api->spi_transfer()`; the outgoing byte argument itself is ignored, there's no full-duplex echo modeling.
- `transfer(void*, size_t)` overwrites the buffer in place, byte by byte, matching real SPI's simultaneous send/receive semantics.
- Backed by a virtual panel similar to I2C's (`spipanel.cpp`): a single configurable byte sequence that `transfer()` cycles through one byte per call, wrapping around when it reaches the end.

#### AVR GPIO Registers
`DDRx`/`PORTx`/`PINx` are injected unconditionally, unlike Wire/SPI/EEPROM/etc., which are only swapped in when the preprocessor sees their `#include`.
- Real sketches almost never write `#include <avr/io.h>` explicitly, it comes in transitively through `Arduino.h`, so there's no include to key off; these globals just always exist.
- Each port (B, C, D) has a shared `AvrPortState{ddr, port, pins[8]}`, where `pins[bit]` maps a register bit to its Arduino pin number, or `-1` if that bit isn't exposed on a real Uno/Nano (e.g. Port B's top two bits are the crystal pins).
- `DDRB`/`PORTB`/`PINB` etc. are thin proxy structs (`AvrDdrReg`, `AvrPortReg`, `AvrPinReg`) holding a pointer to the shared state, with `operator uint8_t()` and `=`/`|=`/`&=`/`^=` overloaded so they read and assign like real memory-mapped registers.
- Writing `DDRx` calls `pinMode()` per changed bit, `OUTPUT` if now set, otherwise `INPUT` or `INPUT_PULLUP` depending on the current `PORTx` bit.
	- Mirrors real AVR: `DDR=0` + `PORT=1` is a pull-up input.
- Writing `PORTx` on an output-configured bit calls `digitalWrite()`.
	- On an input-configured bit it instead calls `pinMode()` to toggle the pull-up, again matching real AVR, where writing `PORTx` on an input pin controls its pull-up rather than driving the pin.
- Reading `PINx` polls `digitalRead()` per exposed bit and packs the result live; it always reflects current electrical state rather than a stored value.
- Writing `PINx` reproduces a genuine AVR quirk: it doesn't set bits, it toggles the corresponding `PORTx` bits (`avr_apply_port(*st, st->port ^ v)`).

```c++
struct AvrPinReg {
    AvrPortState* st;
    operator uint8_t() const { /* pack digitalRead() per exposed bit */ }
    // Real AVR quirk: writing 1 to PINx toggles the corresponding PORTx bit.
    AvrPinReg& operator=(uint8_t v) { avr_apply_port(*st, st->port ^ v); return *this; }
};
```

#### AVR Timer Registers
Unlike GPIO registers, hardware timers need actual time-based emulation. Timer1 (16-bit) and Timer2 (8-bit) both free-run at a rate set by their prescaler, firing overflow/compare-match interrupts as they cross certain values, and PWM output on `OCRxA`/`B` writes.

- Virtual tick counting is done lazily, not by a running counter.
	- `AvrTimerState` stores a reference point (`ref_ticks`, `ref_avr_us`) and a prescaler; the current tick count is computed on demand as `ref_ticks + (elapsed_us * 16) / prescaler` (16 = `F_CPU` in MHz).
	- Any register read/write rebases this reference point first, so changing the prescaler mid-run preserves elapsed time under the old rate rather than jumping.
- Overflow/compare detection uses crossing-counting, not equality checks, because a polling thread can easily skip past the exact tick where `TCNT == OCRA`.
	- `avr_count_crossings()` computes how many times the free-running counter passed a target value (mod the counter's modulus) between the previous poll and now.
	- So ISRs still fire correctly (potentially more than once per poll) even if the poll interval missed the exact moment.
- A dedicated 1ms-resolution `timer_thread_` (started lazily on first `TCCRB`/`TIMSK` write via `ensure_timer_thread_running()`) polls both timers and dispatches `TIMERx_OVF_vect`/`TIMERx_COMPA_vect`/`TIMERx_COMPB_vect` through the same `dispatch_isr()` used elsewhere.
	- Gated by the corresponding `TOIE`/`OCIEA`/`OCIEB` bits in `TIMSK`.
- Writing `OCRA`/`OCRB` also directly drives `analogWrite()` on that timer's associated PWM pin (`pinA`/`pinB`), clamped to 0-255.
	- So `OCR1A = 128` both sets the compare register and immediately updates the canvas's PWM display, without waiting for a timer tick.
- Writing `TCNT` directly rebases the reference point to the written value with no spurious crossings counted.
- Writing `TCCRB` rebases the reference point under the old prescaler before applying the new one, preserving elapsed time across a prescaler change.
- Timer1's prescaler table is `{0,1,8,64,256,1024,0,0}`; Timer2's is `{0,1,8,32,64,128,256,1024}`.
	- Decoded from the low 3 bits of `TCCRxB`, 0 meaning stopped.

```c++
long long avr_count_crossings(long long prevRaw, long long currRaw, long long target, long long modulus) {
    if (currRaw <= prevRaw) return 0;
    return avr_floor_div(currRaw - target, modulus) - avr_floor_div(prevRaw - target, modulus);
}
```
### Circuit Detector
`CircuitDetector::detect()` runs a fixed pipeline over the sketch source: parse `#define`/`const int` symbols and pin arrays, then run three detection tiers in priority order, then fall back to scanning remaining `analogRead()`/`Serial.begin()` calls for anything not already claimed.

- **Tier 1, `detect_pattern`**: source-pattern matching (`.method(`, plain `func(`, or `ClassName ctor(` shapes) via `detect_method_call_pattern`/`detect_wrapper_function_pattern`/`detect_constructor_pattern`.
- **Tier 2, `detect_multi`**: pin-role grouping for multi-pin components, dispatched by a `MultiPinStrategy` enum (`Suffix`, `Prefix`, `Array`, `Singleton`, `None`) on each `ComponentDefinition`.
- **Tier 3, `detect_single`**: keyword fallback against remaining `pinMode()` calls, via `ComponentRegistry::find_by_single_keyword()`, falling back further to a generic `"GenericOutput"`/`"GenericInput"` if nothing matches.
- Keypad matrices, DHT sensors, and MAX7219 chains are hand-written detectors that sit alongside the three generic tiers rather than fitting their strategy enums (independent row/col counts, a non-pin second constructor argument, positional-literal constructor args).
- Conflicts are resolved by tier priority and first-claim.
	- Pins are tracked in a `claimed` set; if a lower-priority match targets an already-claimed pin, it's dropped.
	- A `"Pin N is used by both 'X' and 'Y'"` warning is surfaced in the UI rather than silently overwriting the claim.

`ComponentRegistry` is a flat registry of `ComponentDefinition`s (keywords for all three tiers, pin-role lists, a `create_item` factory, wire color, etc.) that both `CircuitDetector` and `CanvasWidget` iterate, replacing what used to be per-component enum/switch logic. A component can register more than one `ComponentDefinition` under the same `type_name` when it needs more than one strategy. For example, `ColorSensor` registers both a `Singleton` entry (for `#define`-based sketches) and an `Array` entry (for sketches that declare `S2[]`-style pin arrays instead), the same pattern `HBridgeMotor` uses for its bare `ENA`/`IN1`/`IN2` pins.
### Error UX
Compile and runtime errors surface through three separate channels, with no `QMessageBox` involved for any of them (that's reserved for the About dialog and the autosave-restore prompt).

- **Pre-compile linting** (`SketchLinter::checkSource()`): a set of regex-based checks over the sketch text.
	- EX: pin-out-of-range for the selected board, `analogWrite()` on a non-PWM pin, `delay()` inside an ISR, missing `volatile` on ISR-shared variables, `digitalWrite()` without a matching `pinMode()`.
	- These warnings are appended as plain text to the Serial Monitor before compiling; they are not shown as editor highlights.
- **Compiler error parsing**: `Compiler::compile()` pre-checks for a missing `setup()`/`loop()` before invoking g++ at all, then parses g++'s `file:line:col: error|warning: message` output into structured `CompileError`s.
	- Before display, `MainWindow` strips the internal temp-file path.
	- Re-offsets line numbers by the injected-header's line count so they point back at the user's actual source.
	- Restores dot-notation (`Serial.println` instead of the preprocessor's `Serial_println`) and strips the injected `api->` prefix.
	- `SketchLinter::humanizeErrors()` then rewrites common GCC-speak (`'X' was not declared in this scope`, `expected ';' before '}' token`) into plainer messages.
- **Inline editor highlighting**: `MainWindow::showCompileErrors()` paints a full-width background highlight on each error/warning line (color from `apptheme.h`, error vs. warning) with the raw message set as a hover tooltip, and auto-jumps the cursor to the first error line.
	- This runs on both failed and successful compiles, since warning-only highlights still need to appear on a sketch that otherwise compiled fine.
	- This is also why a "WARNING:" line printed to the Serial Monitor by the linter won't necessarily correspond to a highlighted editor line: the linter's warnings and the compiler's warnings are two different sources feeding two different display channels.

### Library Injection Files
`strip_includes()` walks a fixed `kLibs` table and replaces each known `#include <X.h>` with either a full library implementation or nothing at all.
- `content` is a generated string constant (e.g. `g_servo_lib`) for a full implementation.
- `content` is `nullptr` for headers like `EEPROM.h`/`avr/io.h`, whose functionality already exists unconditionally in the runtime and injected header.
- Any remaining `#include <X.h>` that isn't in `kLibs` or the `kStdHeaders` allowlist gets a warning, since it likely means a real Arduino library the preprocessor doesn't understand.

- Unconditional injection: `injected_header.inc` (base runtime shims, String class, Serial helpers, `map`/`constrain`, sleep/WDT macros) plus the AVR GPIO and Timer register files are concatenated into every sketch's generated header regardless of `#include`s.
	- Sketches rarely include `avr/io.h` explicitly even though they use `DDRx`/`PORTx`, it normally arrives transitively through `Arduino.h`.
- Conditional injection: `Wire.h`, `SPI.h`, `Servo.h`, `LiquidCrystal.h`, `SoftwareSerial.h`, `Keypad.h`, `DHT.h`, and `LedControl.h` each have a dedicated `.inc` file only spliced in when the sketch's own source includes the matching header.
	- Their optional-argument/overloaded APIs don't map onto a bare function-pointer table and would otherwise bloat every sketch's generated header for no reason.
```c++
std::string Preprocessor::strip_includes(const std::string& source) {
    struct LibEntry {
        const char* header;
        const char* content; // nullptr = just strip silently
    };
    static const LibEntry kLibs[] = {
        { "Servo",          g_servo_lib         },
        { "LiquidCrystal",  g_liquidcrystal_lib },
        { "SoftwareSerial", g_softwareserial_lib },
        { "Wire",           g_wire_lib },
        { "SPI",            g_spi_lib },
        { "Keypad",         g_keypad_lib },
        { "DHT",            g_dht_lib },
        { "LedControl",     g_ledcontrol_lib },
        { "EEPROM",          nullptr },
        { "Arduino",         nullptr },
        { "avr/pgmspace",    nullptr },
        { "avr/interrupt",   nullptr },
        { "avr/io",          nullptr },
        { "util/delay",      nullptr },
        { "avr/wdt",         nullptr },
        { "avr/sleep",       nullptr },
    };

    // Standard C/C++ headers that the host compiler can find — don't warn about these
    static const char* kStdHeaders[] = {
        "stdio.h", "stdlib.h", "string.h", "math.h", "time.h", "stdbool.h",
        "stdint.h", "stddef.h", "assert.h", "limits.h", "float.h", "ctype.h",
        "errno.h", "inttypes.h", "stdarg.h", "stdint.h", "memory.h",
    };

    std::string s = source;

    for (const auto& lib : kLibs) {
        std::string include_str = std::string("#include <") + lib.header + ".h>";
        s = replace_all(s, include_str, lib.content ? lib.content : "");
    }

    // Warn about any remaining #include <X.h> that VEMCODE doesn't know about
	// ...

    return s;
}
```

## UI Components

### Main Window
`MainWindow` is the top level coordinator. It owns every major UI component; the editor panel, canvas panel, sketch thread, and debug panels. It wires them together through Qt signals and slots. It doesn't implement simulation logic itself, it connects the outputs of one system to the inputs of another.

The four main areas are built in `setupMainArea()` and arranged in a horizontal splitter with the editor on the left and a vertical right splitter containing the canvas above the debug panel. The last area is the top toolbar which contains all of the IDE buttons for running, saving, settings, etc.
#### Editor panel
The editor panel is a simple text editor with some added features to make it more like an IDE; line numbers, a code highlighter, and text events:
- **Line numbers** (`linenumberarea.h`):
	- Adds line numbers to the left of each line on the editor.
- **Code highlighter** (`codehighlighter.cpp/h`):
	- Preprocessor:
		- Color: `#c586c0` or pinkish purple.
	- Keywords:
		- EX: `int`, `float`, `void`, `char` ...
		- Color: `#569cd6` or grayish blue.
		- Text: Bold.
	- Arduino:
		- EX: `pinMode()`, `digitalRead()`, `delay()` ...
		- Color: `#dcdcaa` or pale yellow.
		- Text: normal.
	- Constant:
		- EX: `HIGH`, `LOW`, `OUTPUT`, `INPUT_PULLUP` ...
		- Color: `#4fc1ff` or light blue.
		- Text: normal.
	- Number:
		- Color: `#b5cea8` or pale green.
	- String:
		- Color: `#ce9178` or pale orange.
	- Comment:
		- Inline comments and Comment Blocks.
		- Color: `#6a9955` or dark green.
		- Text: Italic.
	- All seven colors above are the dark-theme palette.
		- `setTheme(bool dark)` swaps in a separate light-theme palette so the highlighter stays legible against a light background too.
		- Desaturated/darkened versions of the same hues, e.g. keywords become `#0451a5`.
- **Text events** (`src/ui/editor/linenumberarea.cpp >> EditorWithLines::keyPressEvent()`):
	- Not `MainWindow::eventFilter()` anymore, that function only handles the autocomplete popup and Ctrl+Wheel zoom now.
	- Tabs >> +4 spaces.
	- Enter >>
		- If `'{'` is typed before, return to next line +4 spaces from previous anchor.
		- Otherwise move to next line at previous anchor point.
	- `'}'` >> auto-4 spaces or dedent.
	- Auto-closes `(`, `[`, `{`, `"` and skips over an already-typed closing character instead of doubling it.
	- Configurable keybindings for duplicate-line and comment-toggle (single-line `//` or block-selection wrapping).
	- Typing `.` emits `dotTyped()` to trigger member autocompletion.
#### Canvas Panel
The canvas panel is the space where the generated circuit is drawn from detected components. When it calls `new CanvasWidget()`, it builds the circuit from `canvaswidget.cpp/h` which draws the board and components.

`CanvasWidget` exposes a single generic signal, `inputChanged(int pin, int eventType, QVariant value)`, rather than one signal per component type. `MainWindow::onComponentInput()` dispatches on `eventType` (a `ComponentEventType` enum: `DigitalPress`, `BouncePress`, `AnalogValue`, `PulseUs`, `ColorRGB`, `KeypadWiring`, `KeypadPress`, `DhtReading`, ...) to the matching `sketchThread_->injectPin/injectAnalog/injectButtonBounce/injectColor/injectKeypadPress/injectDht(...)` call. Each interactive component (Button, Pot, Switch, etc.) is its own `ComponentItem` subclass under `src/components/` that emits this signal directly from its own mouse handlers, see [Input Handling](#input-handling).
#### Debug Panel
The debug panel has grown to five tabs, not three; you switch between them like browser tabs, at the top.
- **Serial monitor** (`mainwindow.cpp`):
	- The serial monitor is configurable based on selected board to configure how many serial monitors are supported.
	- EX: the Teensy 4.1 has 3 serial monitors, Arduino Uno has 1 serial monitor.
	- Vertically adjustable to change size.
- **Signal timeline** (`signaltimeline.cpp`):
	- Adds a logic analyzer style graph for a pin.
	- Pins are **not** auto-added; you type a pin number into the field and click "+ Add pin" to start tracking it.
	- `addEvent()` silently ignores any pin that hasn't been added this way.
- **Serial plotter** (`serialplotter.cpp`):
	- A separate tab from the Serial Monitor for graphing numeric values printed over Serial, rather than reading them as text.
- **Variable watch** (`variablewatch.cpp`):
	- Variables must be added first (an "+ Add variable" row, name + type).
	- The sketch then just calls `watch_variable("LABEL", value);` to update the row that's already there, calling `watch_variable()` alone for a name that was never added doesn't make it appear.
	- Connected to `variableChanged()` in sketch thread to update the value on the UI.
	- Tab contains list of the value updating with the label in a **three** column table (Variable, Type, Value).
- **I2C** (`devicespanel.cpp`) and **SPI** (`spipanel.cpp`):
	- The virtual I2C device and SPI byte-sequence panels described in [Wire / I2C](#wire--i2c) and [SPI](#spi) live here as debug tabs, not floating dialogs.
#### Toolbar
The toolbar itself is now a slim strip with just the logo, app title, **Run**, **Stop**, a **Speed** slider, and a board-name label, everything else moved into a real menu bar (File/Edit/View/Run/Help) above it. File/Save/Settings live under **File**; a **Help** menu adds an in-app documentation viewer (Architecture, Sketch Guide, Adding Components, API Reference, Roadmap) that opens the corresponding `docs/*.md` file in a viewer dialog rather than requiring an external editor.
### Canvas Widget
#### Auto Layout
`pinLocation()` maps the pin number to the physical position on the canvas. Analog pins run down the left edge; digital pins run down the right edge, but "digital" means *outside the analog range*, not just *below it*. On boards like the Teensy 4.1 (`pin_count=42`, `analog_offset=14`, `analog_count=18`), the analog block doesn't reach `pin_count`, so pins 32-41 are digital pins sitting *above* the analog block, not below it. `isAnalogPin()`/`digitalPinIndex()`/`digitalPinCount()` exist specifically to treat "below" and "above" digital pins as one contiguous strip down the right edge instead of missing the upper ones.
```c++
bool CanvasWidget::isAnalogPin(int pin) const {
    return pin >= profile_.analog_offset && pin < profile_.analog_offset + profile_.analog_count;
}

int CanvasWidget::digitalPinCount() const {
    int low  = profile_.analog_offset;
    int high = std::max(0, profile_.pin_count - (profile_.analog_offset + profile_.analog_count));
    return low + high;
}

int CanvasWidget::digitalPinIndex(int pin) const {
    if (pin < 0 || pin >= profile_.pin_count || isAnalogPin(pin)) return -1;
    if (pin < profile_.analog_offset) return pin;
    int high_start = profile_.analog_offset + profile_.analog_count;
    return profile_.analog_offset + (pin - high_start);
}

QPointF CanvasWidget::pinLocation(int pin) {
    if (isAnalogPin(pin)) {
        float spacing = (float)BOARD_H / (float)(profile_.analog_count + 1);
        float y = BOARD_Y + spacing * (pin - profile_.analog_offset + 1);
        return QPointF(BOARD_X, y);
    }
    int idx = digitalPinIndex(pin);
    if (idx >= 0) {
        float spacing = (float)BOARD_H / (float)(digitalPinCount() + 1);
        float y = BOARD_Y + spacing * (idx + 1);
        return QPointF(BOARD_X + BOARD_W, y);
    }
    return QPointF(BOARD_X + BOARD_W / 2.0, BOARD_Y);
}
```
`drawBoard()` then renders the board graphics and places pin dots at each location, looping `pin_count` times and skipping analog pins for the digital pass rather than stopping at `analog_offset`. Board/border/label colors (`#1a1a2e`, `#3a3a5c`, `#555577`) and pin-dot colors (`#2a2a3a` fill, `#444466` border) follow the dark theme; a separate light-theme set exists for the board/border/label only (`#d8d8e8`/`#9898b8`/`#5c5c86`), pin dots and all component chrome stay fixed regardless of theme. The chip's own background box (`#3c3c62` fill, `#5a5a89` border) and its label (`#9595d2`, **not** `#333355`, that's the pin-number label color) are also drawn here.
#### Component Rendering
There's no single `drawComponent()` anymore; component sizing and type-specific drawing moved off of `CanvasWidget` entirely. Each component type is its own `ComponentItem` subclass under `src/components/` that owns its own `boundingRect()` and paints itself, `CanvasWidget` just positions the item and routes its wires, it no longer switches on component type at all. `refresh()` runs in two phases so components don't overlap when several land in the same column:

1. **Placement pass**: for each detected component, look up its `ComponentDefinition` in the registry, construct the `ComponentItem` (`def->create_item(...)`), and read `is_output`/wire color/wire pins off the definition rather than a type switch.
	- The item's `boundingRect()` gives `comp_w`/`comp_h`; it varies per component, and per instance for things like a MAX7219 chain with a configurable device count.
	- `comp_w`/`comp_h` decides the column: output components go right of the board, analog inputs go in an outer left column, digital inputs go in an inner left column.
	- A pin the user has manually dragged in layout mode skips this and keeps its saved position instead.
2. **Stacking pass**: sorted by column then by target Y, each component is pushed down past the previous one's bottom edge (plus a small gap) so tightly-packed pins never overlap, before `placeComponent()` actually adds the item to the scene and wires it up.

```c++
void CanvasWidget::placeComponent(const DetectedComponent& comp, const ComponentDefinition* def,
                                   ComponentItem* item, float comp_x, float comp_y,
                                   int comp_w, int comp_h) {
    item->setPos(comp_x, comp_y);
    scene_->addItem(item);

    // Connect BEFORE anything can emit -- configureMultiPin/emitInitialValue
    // below may synchronously emit inputChanged, and Qt drops signals emitted
    // with nothing connected yet rather than buffering them.
    connect(item, &ComponentItem::inputChanged, this, &CanvasWidget::onComponentInput);

    pinItems_[comp.pin] = item;
    if (comp.pins.size() > 1) {
        for (int mp : comp.pins) if (mp >= 0) pinItems_[mp] = item;
        if (comp.rows > 0 && comp.cols > 0) item->configureRowsCols(comp.rows, comp.cols);
        item->configureMultiPin(comp.pins);
    }
    item->emitInitialValue();
    // ... re-apply any manually-configured rotation/color/polarity for this pin ...

    componentInfo_[item] = ComponentInfo{ comp.pin, def->is_output, wire_pins, def->wire_color, {} };
    updateWires(item);
}
```

Wire routing (`updateWires()`, shared by initial placement and by live re-routing while a component is dragged in layout mode) still staggers each wire's attach point by `WIRE_SPACING = 5.0f` so multiple wires into one component don't stack, but the turn point differs by direction, fixed after an earlier version routed the vertical segment through whatever else got stacked between the pin and its component:
```c++
float inter_x;
if (isAnalogPin(wpin)) {
    inter_x = BOARD_X - 20.0f - i * WIRE_SPACING;             // turn near the pin
} else if (info.is_output) {
    inter_x = comp_x - 10.0f - i * WIRE_SPACING;               // turn near the component
} else {
    inter_x = target.x() + 10.0f + i * WIRE_SPACING;           // turn near the pin (mirrors analog)
}
```
#### Input Handling
Per-component interactivity (button clicks, pot drags, switch toggles) is **not** handled in `CanvasWidget` anymore, it lives entirely in each `ComponentItem` subclass under `src/components/`, which overrides Qt's mouse events directly on the graphics item and emits `inputChanged(pin, eventType, value)` itself (e.g. `ButtonItem` emits `BouncePress`, `ButtonCleanItem` emits `DigitalPress`, `PotItem` emits `AnalogValue` on drag). `CanvasWidget`'s own `mousePressEvent()`/`mouseMoveEvent()`/`mouseReleaseEvent()` only handle two things that are genuinely canvas-level, not component-level:

1. **Ctrl+click on a component**: opens a config dialog if the item supports it, rotation (`supportsRotation()`, a 0/90/180/270 picker), color (`supportsColorConfig()`, a `QColorDialog`), or polarity (`supportsPolarity()`, common-cathode/anode). Whatever's picked is applied to the item immediately and remembered (keyed by pin) so it survives the next `refresh()`.
2. **Layout-mode drag**: when `setLayoutMode(true)` is active, a left-click-drag on any component just repositions it and re-routes its wires via `updateWires()`; releasing saves the dropped position into `manualPositions_` so a later `refresh()` (e.g. after editing and re-running the sketch) doesn't undo it.

```c++
void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier)) {
        ComponentItem* comp = /* walk up parentItem() chain from itemAt(event->pos()) */;
        if (comp && comp->supportsRotation())  { /* QInputDialog: 0/90/180/270, then comp->configureRotation(...) */ }
        if (comp && comp->supportsColorConfig()) { /* QColorDialog, then comp->configureColor(...) */ }
        if (comp && comp->supportsPolarity())  { /* QInputDialog: cathode/anode, then comp->configurePolarity(...) */ }
        event->accept();
        return;
    }

    if (layoutMode_ && event->button() == Qt::LeftButton) {
        ComponentItem* comp = /* same walk-up */;
        if (comp) {
            draggedItem_ = comp;
            dragOffset_  = comp->pos() - mapToScene(event->pos());
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (draggedItem_) {
        draggedItem_->setPos(mapToScene(event->pos()) + dragOffset_);
        updateWires(draggedItem_); // re-route this component's wires live while dragging
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (draggedItem_) {
        manualPositions_[componentInfo_[draggedItem_].primary_pin] = draggedItem_->pos();
        draggedItem_ = nullptr;
        setCursor(layoutMode_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
```
## Data Flow
The following traces a single `digitalWrite(13, HIGH);` call from the user's sketch, to the canvas and signal timeline to show how every layer of the system connects.
```
User sketch calls digitalWrite(13, HIGH) during run...
→ Preprocessor converted digitalWrite( to api->digitalWrite(
→ api->digitalWrite( points to impl_digitalWrite( to fire in arduino runtime
→ pin_values[13] gets updated to the new value (HIGH)
→ on_pin_changed(13, 1) fires to start the connection to UI
→ SketchThread emits pinChanged(13, 1) to connect to MainWindow::onPinChanged()
→ onPinChanged(13,1) receives it to: 
	→ update canvasWidget_ to call updatePin()
	→ set pin item to the active color
	→ update the signal timeline to addEvent(13, 1, elapsed)
```
Every API call in the sketch follows the same path; the function pointer table routes it into the runtime, it updates state and fires a callback, and propagates up through the sketch thread to the UI.
## Board Profiles
The board profiles are stored in boardprofile.h which just includes a structure for board profiles including all necessary variables that need set. Create board profiles for supported boards under the struct to save default boards to be set.
```c++
#pragma once

struct BoardProfile {
    const char* name;
    const char* chip;
    int pin_count;
    int analog_offset;
    int analog_count;
    int pwm_resolution;
    int serial_count;  // number of hardware serial ports (api supports max 3)
};

static const BoardProfile BOARD_UNO = 
{"Arduino Uno", "ATmega328P", 20, 14,  6,  255, 1};

static const BoardProfile BOARD_NANO = 
{"Arduino Nano", "ATmega328P", 22, 14,  8,  255, 1};

static const BoardProfile BOARD_MEGA = 
{"Arduino Mega 2560", "ATmega2560", 70, 54, 16,  255, 3};

static const BoardProfile BOARD_DUE = 
{"Arduino Due", "AT91SAM3X8E", 66, 54, 12, 4095, 3};

static const BoardProfile BOARD_TEENSY = 
{"Teensy 4.1", "IMXRT1062", 42, 14, 18, 4095, 3};
```
You can select a different board from default by two ways:
- Writing `// @board <name>` in the sketch.
- Using Settings to select from a list of supported boards.
## Adding Components
See [ADDING_COMPONENTS.md](ADDING_COMPONENTS.md) for adding components/api functions.

## Headless Mode
Running `vemcode <sketch.cpp>` (any args after the sketch path) builds a `QCoreApplication` instead of `QApplication` and drives the exact same compile, detect, and run pipeline on a single thread with no windows involved. This is what the test suite runs against.

- `key=value` args set `timeout=`, `speed=`, or `timeline=true`; a bare token is the optional `.timeline` path.
	- `timeline=true` with no explicit path derives the sibling `.timeline` file by swapping the sketch's extension.
- A `.timeline` file is a plain-text test script: `time, VERB, TARGET, args...`.
	- Comments start with `#` (respecting quoted strings so a `#` inside `"..."` isn't treated as one).
	- Fields can be quoted to allow embedded commas, and lines are sorted by timestamp after parsing regardless of file order.
- The main loop pairs `TestRunner::fireDueEvents()` with `host.run_loop()` one-to-one, once each per iteration.
	- `fireDueEvents()` deliberately fires at most one due event per call rather than a batch of everything that's become due.
	- This guarantees the sketch gets a chance to react (re-read an injected pin, re-drive an output) between any two timeline events.
	- This matters because `speed=N` compresses real time between sketch-time events; firing a batch could let an ASSERT race ahead and check stale state.
- Two ASSERT targets exist: `PIN` (compares injected/read pin state to `HIGH`/`LOW`/an int) and `SERIAL_CONTAINS` (substring match against everything written to Serial so far).
	- Each prints PASS/FAIL immediately and accumulates into the final summary.
- Action targets dispatch by the matching `DetectedComponent`'s type (`Button`, `ButtonClean`, `Switch`, `Potentiometer`/analog sensors, `DistanceSensor`, `Joystick`, `ColorSensor`).
	- Or by reserved names (`SERIAL`, `WIRE`, `SPI`, or a bare pin number for SoftwareSerial injection).
	- Each routes to the corresponding `SketchHost::inject_*` call.
- `Button` vs. `ButtonClean` matters here too.
	- `Button` goes through `inject_button_bounce()` (the same ~10ms random-bounce window a real click gets).
	- `ButtonClean` uses `inject_pin()` directly, exempt from bounce, consistent with how the canvas's mouse handlers treat the two component types differently.
- `printSummary()` reports passed/total assertions and warns if events never fired (ran out of time).

## Building & Testing
### Build Scripts
Three scripts under `scripts/`, all `cd` to the repo root first:
- `run.sh`: thin wrapper, `cmake --build build` then execs `./app/VEMCODE "$@"`, forwarding all arguments (so `./scripts/run.sh Foo.cpp` runs it headlessly).
	- Assumes `cmake -B build -S .` has already been run once.
- `clean.sh`: removes generated per-sketch build artifacts under `app/sketches/`, `*.so`/`*.tmp.so`, `*.dll`/`*.tmp.dll`, `*.dylib`/`*.tmp.dylib`, and `_vb_temp.cpp`, matching what `.gitignore` excludes.
- `run_tests.sh`: the test runner, see Test Suite below.
	- There's no separate Qt-deployment script; packaging a distributable build is a manual step.

### Test Suite
There's no CTest/`enable_testing()` wiring in `CMakeLists.txt`; testing is driven entirely by `scripts/run_tests.sh` run manually. Fixtures live at `app/sketches/tests/<Name>/<Name>.cpp`, each optionally paired with a `<Name>.timeline`.

- The script builds first, then for each fixture with a `.timeline` runs `timeout -k 2 --signal=INT 15s $VEMCODE_BIN $sketch timeline=true speed=10`.
	- A pass is exit code 0, meaning every ASSERT in the timeline passed, per the `TestRunner`/headless-mode convention.
- Fixtures with no `.timeline` are treated as smoke tests instead: they're expected to loop forever.
	- A pass is exit 0, 124 (timed out), or 137 (SIGKILL after the `-k 2` grace period).
- Child output is streamed live rather than captured via command substitution, which would otherwise buffer silently until the process exits.
- Prints a final `PASSED (n)`/`FAILED (n)` summary and exits nonzero if anything failed.

### Windows Cross-Build
The documented, supported Windows build is native: MSYS2's UCRT64 toolchain on Windows itself (`pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-qt6-base ...`), not a Linux-hosted cross-compile.
- Qt's own bundled MinGW toolchain is missing `std::filesystem`/`std::thread`/`setjmp` runtime symbols on Windows, so the UCRT64 toolchain is required instead of whatever ships with Qt.

A separate `/build-win64/` tree exists locally as a gitignored, manually-configured mingw-w64 cross-build (Fedora's system `mingw64-*` toolchain file), used for local sanity-checking rather than the documented release path. Portability fixes needed to get either build working:
- `SketchHost`'s crash-recovery signal handling (`SIGFPE`/`SIGSEGV` around the sketch execution loop) is duplicated behind `#ifdef _WIN32`.
	- Windows has no `sigaction`/`sigjmp_buf`/`sigsetjmp`/`siglongjmp`, so the Windows path uses plain `signal()`/`jmp_buf`/`setjmp()`/`longjmp()` instead.
	- Relies on the CRT resetting the handler to `SIG_DFL` before invoking it (equivalent to `SA_RESETHAND` on the POSIX side).
- MinGW builds link `-static-libgcc -static-libstdc++` and don't export DLL symbols by default the way a Linux `.so` does.
	- Variable Watch's `dlsym`-by-name polling depends on that for every sketch global, not just the `VB_EXPORT`-marked `vb_init`/`setup`/`loop`.
- `dl` is only linked on `UNIX AND NOT APPLE`.
	- On `WIN32`, Release builds set `WIN32_EXECUTABLE` to hide the console window while Debug builds keep it for `std::cout` visibility.