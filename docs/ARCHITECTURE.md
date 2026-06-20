# VEMCODE : Architecture

**Overview:**
This document covers a more in depth viewpoint about how VEMCODE actually works in the back-end. If you want to learn about how the current runtime executes the functions at a base level using the pointer table, understand how the configuration of the UI is developed and connected to the runtime, and more intricate details that is explained here.

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
How it transforms sketch syntax to shared library format
- Transform ISR blocks and assembly code
	- EX: `ISR(vect_name)` >> `void __vb_isr_X_vect() { body }`
	- EX: `cli` >> `noInterrupts()`
- Replace functions with api call
	- EX: `digitalRead(` >> `api->digitalRead(`
- Strip includes and replace them if needed
	- EX: `#include <avr/io>` >> `null`
	- EX: `#include <Servo>` >> custom `servo.h` file injected
- Generate forward declarations like Arduino IDE does automatically 
- Wrap setup and loop with DLL exports
- Inject register_isr() calls into setup() for each found ISR vector
- Inject a safety delay to prevent freezes
- Inject custom header to cover string class, math functions, and other default functions needed
### Compiler
How g++ is invoked, flags, output location, error capture
- Create the sketch path with appropriate library format 
	- EX: .so for linux, .dll for windows
- Write the sketch file to the dll path
- Pre check if setup and loop are included before wasting compile time
- Run the preprocessor
- Extract the board name hint and surface it to the caller 
	- EX: `// @board <name> `
- Create the temp file .cpp for hot reloading
	- EX: `SKETCH_vb_temp.cpp`
- Collect any extra .cpp files in the sketch folder to add them to the compile command
- Write the command to the ostringstream to run the compile
- Read the results and parse errors if there are any then return
### Sketch Host
dlopen/LoadLibrary, temp copy strategy, hot-reload mechanism
- First free any previous loaded libraries
- Copy to a temp file for hot reloading
	- EX: `SKETCH.tmp.dll` or `SKETCH.tmp.so`
- Open using dlopen on linux or LoadLibraryA on windows
- Extract vb_init/setup/loop, dlsym on linux and GetProcAdress on windows
- Hosts connections to runtime and runs the loop
  EX: `SketchHost::inject_pin` >> `runtime_.inject_pin`
### Sketch Thread
Background thread, loop execution model, stop mechanism
- connect the runtime to arduino runtime through sketchhost
- Override outputs to emit signals instead of printing to console
- Check to see if variable changed and emit that signal
- Check LCD text output and emit
- Load the sketch DLL which calls vb_setup() or void setup() to run
- Run the void loop() if the host is running and check for crashes
	- Check for reloaded sketches as well and emit that signal to reload
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
    // 1. Update pin state, bail if unchanged
    int old_value = g_runtime->state_.pin_values[pin];
    g_runtime->state_.pin_values[pin] = value;
    if (old_value == value) return;

    // 2. Fire canvas/timeline callback
    if (g_runtime->on_pin_changed)
        g_runtime->on_pin_changed(pin, value);

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
All timing functions scale by speed_multiplier_ which is set from the speed slider (`1/speed`). `delay()` sleeps in 10ms chunks so stop requests are handled faster than waiting for the sketches full blocking duration.
```c++
void impl_delay(unsigned long ms) {
    unsigned long scaled = ms * speed_multiplier_;
    // sleep 10ms at a time, check stop_requested_ between each chunk
}

unsigned long impl_millis() {
    auto real_us = steady_clock::now() - state_.start_time;
    return real_us / speed_multiplier_ / 1000;
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
#### Misc
`tone()` stores the frequency in `tone_frequencies_` and fires `on_pin_changed` so the canvas can show the buzzer active. If a duration is specified it spins up a detached thread that sleeps for the scaled duration then clears the tone.

`pulseIn()` has three paths;
- if a pulse duration was injected from the UI it returns immediately
- if the pin belongs to a color sensor it reads from the color channel map
- otherwise it falls back to a three-phase polling loop 
	- (wait for idle >> wait for pulse start >> measure pulse end).

`watch_variable()` fires `on_variable_changed` which routes to the Variable Watch panel, or falls back to stdout.
### Circuit Detector
How component detection works, the three detection tiers (after 7b)
TODO: need to finish 7b
## UI Components

### Main Window
`MainWindow` is the top level coordinator. It owns every major UI component; the editor panel, canvas panel, sketch thread, and debug panels. It wires them together through Qt signals and slots. It doesn't implement simulation logic itself, it connects the outputs of one system to the inputs of another.

The three main areas are built in `setupMainArea()` and arranged in a horizontal splitter with the editor on the left and a vertical right splitter containing the canvas above the debug panel.
#### Editor panel
The editor panel is a simple text editor with some added features to make it more like an IDE; line numbers, a code highlighter, and text events:
- **Line numbers** (`linenumberarea.h`):
	- Adds line numbers to the left of each line on the editor
- **Code highlighter** (`codehighlighter.cpp/h`)
	- Preprocessor:
		- Color: `#c586c0` or pinkish purple
	- Keywords:
		- EX: `int`, `float`, `void`, `char` ...
		- Color: `#569cd6` or grayish blue
		- Text: Bold
	- Arduino:
		- EX: `pinMode()`, `digitalRead()`, `delay()` ...
		- Color: `#dcdcaa` or pale yellow
		- Text: normal
	- Constant:
		- EX: `HIGH`, `LOW`, `OUTPUT`, `INPUT_PULLUP` ...
		- Color: `#4fc1ff` or light blue
		- Text: normal
	- Number:
		- Color: `#b5cea8` or pale green
	- String:
		- Color: `#ce9178` or pale orange
	- Comment:
		- Inline comments and Comment Blocks
		- Color: `#6a9955` or dark green
		- Text: Italic
- **Text events** (`mainwindow.cpp/h >> Mainwindow::eventFilter()`)
	- Tabs >> +4 spaces
	- Enter >> 
		- if `'{'` is typed before, return to next line +4 spaces from previous anchor
		- otherwise move to next line at previous anchor point
	- `'}'` >> auto-4 spaces or dedent
#### Canvas Panel
The canvas panel is the space where the generated circuit is drawn from detected components. When it calls `new CanvasWidget()`, it builds the circuit from `canvaswidget.cpp/h` which draws the board and components.

We then run the connections from inputting values on the UI (`poteniomenterChanged`, `buttonPressed`, etc.) and connecting them to the `sketchthread` to inject the values. This is an early chain in the connection from the UI to the `sketchthread`.
#### Debug Panel
The debug panel is the location of the three debug tabs; Serial monitor(s), Signal timeline, and Variable watch. You can switch between the tree tabs like you would a web browser with the tabs located at the top to click on to select desired tab.
- **Serial Monitor(s)** (`mainwindow.cpp`)
	- The serial monitor is configurable based on selected board to configure how many serial monitors are supported. 
	- EX: the Teensy 4.1 has 3 serial monitors, Arduino Uno has 1 serial monitor. 
	- Vertically adjustable to change size.
- **Signal Timeline** (`signaltimeline.cpp`)
	- Adds a logic analyzer style graph for each digital pin
	- Each pin is added automatically if it has a changing value
- **Variable Watch**
### Canvas Widget
#### Auto Layout
How pinLocation() works, the two-column layout (outputs right, inputs left), 
analog vs digital column separation, wire routing (3-segment for analog, 
staggered intermediate x for digital)
#### Component Rendering
drawComponent() — how each component type gets its box, colors, child labels, 
sensor input fields (QLineEdit proxies), the componentColor() active/inactive map
#### Input Handling
mousePressEvent/mouseReleaseEvent/mouseMoveEvent — button press/release with 
bounce vs clean, switch toggle, potentiometer drag with value interpolation

## Data Flow
Walk through a specific example end to end — like digitalWrite(13, HIGH) from sketch to canvas

## Board Profiles
How profiles drive pin count, analog mapping, canvas

## Adding Components
Brief note pointing to the dev generator after Phase 7b