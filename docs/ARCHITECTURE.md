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

The four main areas are built in `setupMainArea()` and arranged in a horizontal splitter with the editor on the left and a vertical right splitter containing the canvas above the debug panel. The last area is the top toolbar which contains all of the IDE buttons for running, saving, settings, etc.
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

We then run the connections from inputting values on the UI (`potentiometerChanged`, `buttonPressed`, etc.) and connecting them to the `sketchthread` to inject the values. This is an early chain in the connection from the UI to the `sketchthread`.
#### Debug Panel
The debug panel is the location of the three debug tabs; Serial monitor(s), Signal timeline, and Variable watch. You can switch between the three tabs like you would a web browser with the tabs located at the top to click on to select desired tab.
- **Serial Monitor(s)** (`mainwindow.cpp`)
	- The serial monitor is configurable based on selected board to configure how many serial monitors are supported. 
	- EX: the Teensy 4.1 has 3 serial monitors, Arduino Uno has 1 serial monitor. 
	- Vertically adjustable to change size.
- **Signal Timeline** (`signaltimeline.cpp`)
	- Adds a logic analyzer style graph for each digital pin
	- Each pin is added automatically if it has a changing value
- **Variable Watch**
	- Add a variable to watch by adding this line of code to the sketch like a print:
		- `watch_variable("LABEL", value);`
	- Connected to `variableChanged()` in sketch thread to update the value on the UI
	- Tab contains list of the value updating with the label in a two column table
#### Toolbar
This is the top slim panel spanning the whole width that contains the following buttons in this order; Run, Stop, Speed slider, New Sketch, Open Sketch, Recent Sketch, Save Sketch, Settings. This is where all the main interaction for using VEMCODE exists besides programming and interacting with the circuit.
### Canvas Widget
#### Auto Layout
`pinLocation()` maps the pin number to the physical position on the canvas. Digital pins run down the right edge of the board, analog on the left. `drawBoard()` then renders the board graphics and places pin dots at each location using the board profile's `analog_offset` and `analog_count` to drive the loops.
```c++
QPointF CanvasWidget::pinLocation(int pin) {
	// if the pin is a digital pin
    if (pin >= 0 && pin < profile_.analog_offset) {
        float spacing = (float)BOARD_H / (float)(profile_.analog_offset + 1);
        float y = BOARD_Y + spacing * (pin + 1);
        return QPointF(BOARD_X + BOARD_W, y);
    }
    // if the pin is a analog pin
    if (pin >= profile_.analog_offset && pin < profile_.analog_offset + profile_.analog_count) {
        float spacing = (float)BOARD_H / (float)(profile_.analog_count + 1);
        float y = BOARD_Y + spacing * (pin - profile_.analog_offset + 1);
        return QPointF(BOARD_X, y);
    }
    return QPointF(BOARD_X + BOARD_W / 2.0, BOARD_Y);
}

void CanvasWidget::drawBoard() {
	// Setup the board (consistent width, variable height)
	// - Board Color: #1a1a2e(dark gray blue), Border #3a3a5c(gray blue)
	// - Labels:
	//     - Board label: Courier new, 9, #555577(light gray blue)
	//     - Chip Label: Courier new, 8, #333355(gray blue)

    // Digital pins
    for (int i = 0; i < profile_.analog_offset; i++) {
        QPointF pos = pinLocation(i);
        // Pin Color: #2a2a3a(dark gray blue), Border #444466(gray blue)
        // Label: Courier new, 7
    }

    // Analog pins
    for (int i = profile_.analog_offset; i < profile_.analog_offset + profile_.analog_count; i++) {
        QPointF pos = pinLocation(i);
        // Pin Color: #2a2a3a(dark gray blue), Border #444466(gray blue)
        // Label: Courier new, 7, add a A to the number
    }
}
```
#### Component Rendering
`drawComponent()` has two base stages, drawing the detected component in the right spot and connecting the component with wires to the correct pins:
- Drawing detected component:
	1) Set output components to go on the right, inputs on the left
	2) Set component Dimensions: width = 100, and height depending on component
	3) Align the Y with pin position on the board
	4) Draw component box
	5) Set component input fields if needed
		- i.e. if/else ladder to set component input events with text or mouse events
- Drawing connecting wires:
	- Offset: `15.0f + i * WIRE_SPACING` where `WIRE_SPACING = 5.0f`
	1) Start at the pin and draw a horizontal line stopping at the midpoint with the offset
	2) Draw vertical wire (up|down) to meet component y with the offset
	3) Draw a horizontal line to the component edge to meet the component
```c++
void CanvasWidget::drawComponent(const DetectedComponent& comp) {
    // Output components go on right side, input on left
    bool is_output = (comp.type == ComponentType::LED || ...);

	// Component dimensions
    int comp_w = 100;
    int comp_h = (comp.type == ComponentType::ColorSensor)  ? 64
               : (comp.type == ComponentType::HBridgeMotor) ? 54
               : (comp.type == ComponentType::LCD)          ? 54
               : 44;
	
	// Column Placement
    float comp_x;
    if (is_output) { comp_x = BOARD_X + BOARD_W + 80; } 
    else if (is_analog_input) {comp_x = BOARD_X - comp_w - 180; } 
    else { comp_x = BOARD_X - comp_w - 80; }
    
    // Align Y with the actual pin position on the board
    float comp_y = pin_pos.y() - comp_h / 2.0f;

    // Component box -- store pinItems_ keyed by pin
    QGraphicsRectItem* rect = new QGraphicsRectItem(0, 0, comp_w, comp_h);
    rect->setBrush(QBrush(componentColor(comp.type, false)));
    scene_->addItem(rect);
    pinItems_[comp.pin] = rect;
	
	// Button Example (Full)
	// Switch, ButtonClean follow same pattern
    if (comp.type == ComponentType::Button) {
        buttonStates_[comp.pin] = false;
        rect->setFlag(QGraphicsItem::ItemIsSelectable);
        rect->setAcceptHoverEvents(true);
        rect->setCursor(Qt::PointingHandCursor);
        rect->setToolTip("Click to press, release to let go");
    }
	
	// Distance sensor (shortened)
	// converts cm text input to microseconds to be interpreted by code
    if (comp.type == ComponentType::DistanceSensor) {
        QLineEdit* input = new QLineEdit();
        QGraphicsProxyWidget* proxy = scene_->addWidget(input);
        proxy->setParentItem(rect);
        connect(input, &QLineEdit::textChanged, this, [this, echo_pin]
	        (const QString& text) { 
		    emit pulseInjected(echo_pin, cm * 2.0f / 0.034f);
		});
    }

    // Same pattern for other Components...
	
	// Drawing connecting wires
    for (int wpin : wire_pins) {
        if (wpin < 0) { i++; continue; }

        QPointF target = pinLocation(wpin);

        // Each wire attaches at a different y so they don't stack
        float attach_y = comp_y + 15.0f + i * WIRE_SPACING;

        QPointF comp_edge = is_output
            ? QPointF(comp_x, attach_y)
            : QPointF(comp_x + comp_w, attach_y);

        bool pin_is_analog = (wpin >= profile_.analog_offset);
        if (pin_is_analog) {
            // Analog: 3-segment route around board left edge, staggered per wire
            float inter_x = BOARD_X - 80 - i * WIRE_SPACING;
            QPointF mid1(inter_x, target.y());
            QPointF mid2(inter_x, attach_y);
            drawWire(target, mid1);
            drawWire(mid1, mid2);
            drawWire(mid2, comp_edge);
        } else {
            // Digital: horizontal from pin, vertical at staggered x, horizontal into component
            float inter_x = is_output
                ? comp_x - 10.0f - i * WIRE_SPACING
                : comp_x + comp_w + 10.0f + i * WIRE_SPACING;
            QPointF mid1(inter_x, target.y());
            QPointF mid2(inter_x, attach_y);
            drawWire(target, mid1);
            drawWire(mid1, mid2);
            drawWire(mid2, comp_edge);
        }
        i++;
    }
}
```
#### Input Handling
This is handled by three main functions, `mouseMoveEvent()`, `mousePressEvent()`, `mouseReleaseEvent()`:
- **`mousePressEvent()`**
	- Walk up the chain of events to check stacked items like a text label on top of rect
	- If stack check for types of components to update the state, 
		- EX: if button is clicked...
			- Set button state to true
			- Change component color to the active state
			- emit the buttonBounced() as 0 on the pin to register as pressed
		- Same logic for rest of clickable components (ButtonClean, Switch, Pot)
	- Update the event
```c++
void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    QGraphicsItem* item = itemAt(event->pos());

    // Walk up parent chain -- click may land on text label on top of rect
    while (item && !dynamic_cast<QGraphicsRectItem*>(item))
        item = item->parentItem();

    if (!item) { QGraphicsView::mousePressEvent(event); return; }

    int pin = pinItems_.key(
        dynamic_cast<QGraphicsRectItem*>(item), -1);
	
    if (pin >= 0 && pinTypes_.value(pin) == ComponentType::Button) {
        buttonStates_[pin] = true;
        dynamic_cast<QGraphicsRectItem*>(item)->setBrush(
            QBrush(componentColor(ComponentType::Button, true)));
        emit buttonBounced(pin, 0); // LOW = pressed; bounced path
        return;
    }

    if (pin >= 0 && pinTypes_.value(pin) == ComponentType::ButtonClean) {
        // Same as Button
        emit buttonPressed(pin, 0); // LOW = pressed; clean path
        return;
    }

    if (pin >= 0 && pinTypes_.value(pin) == ComponentType::Switch) {
        bool new_state = !switchStates_.value(pin, false);
        switchStates_[pin] = new_state;
        // Rest is the same as Button
    }

    if (pin >= 0 && pinTypes_.value(pin) == ComponentType::Potentiometer) {
        dragPin_        = pin;
        dragStartY_     = event->pos().y();
        dragStartValue_ = analogValues_.value(pin, 512);
        dynamic_cast<QGraphicsRectItem*>(item)->setCursor(Qt::SizeVerCursor);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}
```
- **`mouseReleaseEvent()`**
	- If it is a drag pin, reset the dragPin_ state and update the release event
	- If it is a button type (button, Clean button)
		- Check if button is true, if it is:
			- Set it false
			- Reset the component color
			- Emit function with value at 1 on the pin for release 
		- Update the mouse release event to cover the rest of the cases
```c++
void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (dragPin_ >= 0) {
        dragPin_ = -1;
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    for (auto pin : buttonStates_.keys()) {
        if (buttonStates_[pin]) {
            buttonStates_[pin] = false;
            ComponentType t = pinTypes_.value(pin);
            pinItems_[pin]->setBrush(QBrush(componentColor(t, false)));
            if (t == ComponentType::ButtonClean)
                emit buttonPressed(pin, 1);
            else
                emit buttonBounced(pin, 1); // bounced release
        }
    }
    QGraphicsView::mouseReleaseEvent(event);
}
```
- **`mouseMoveEvent()`**
	- if the drag pin was reset, return
	- Check the delta of the starting y, and new event: `event->pos().y()`
	- Calculate the new value using qBound to keep the value bounded between 0-1023
	- Set the analog value to update the runtime
	- Change the color of the component to a ratio of the set value (i.e. change brightness of pot based on value, higher value is brighter color)
	- emit to connect to the mainwindow to be connected the runtime down the chain.
```c++
void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragPin_ < 0) { QGraphicsView::mouseMoveEvent(event); return; }

    int delta = dragStartY_ - event->pos().y();
    int new_value = qBound(0, dragStartValue_ + delta * 4, 1023);

    analogValues_[dragPin_] = new_value;

    auto it = pinItems_.find(dragPin_);
    if (it != pinItems_.end()) {
        float ratio = new_value / 1023.0f;
        QColor active = componentColor(ComponentType::Potentiometer, true);
        QColor inactive = componentColor(ComponentType::Potentiometer, false);
        int r = inactive.red()   + ratio * (active.red()   - inactive.red());
        int g = inactive.green() + ratio * (active.green() - inactive.green());
        int b = inactive.blue()  + ratio * (active.blue()  - inactive.blue());
        it.value()->setBrush(QBrush(QColor(r, g, b)));

        it.value()->setToolTip(
            QString("Value: %1 / 1023").arg(new_value));
    }

    emit potentiometerChanged(dragPin_, new_value);
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
- Writing `// @board <name>` in the sketch
- Using Settings to select from a list of supported boards.
## Adding Components
See [ADDING_COMPONENTS.md](ADDING_COMPONENTS.md) for adding components/api functions.