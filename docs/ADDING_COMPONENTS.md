# VEMCODE : Adding Components

**Overview:**
This is a guide for adding a new simulated component to VEMCODE, a sensor, an actuator, whatever. Adding one is almost always a single new file under `src/components/`: a `ComponentDefinition` describing how to detect it from sketch source, and a `ComponentItem` subclass describing how it looks and behaves on the canvas. There's no central list to edit, a file self-registers just by being compiled in, CMake globs `src/components/*.cpp` automatically.

## Table of Contents
- [VEMCODE : Adding Components](#vemcode--adding-components)
  - [Table of Contents](#table-of-contents)
  - [Registering a Component](#registering-a-component)
  - [Detection](#detection)
    - [Keyword Matching](#keyword-matching)
    - [Multi-Pin Groups](#multi-pin-groups)
    - [Paired Defines](#paired-defines)
    - [Custom Detection](#custom-detection)
    - [Detection Priority](#detection-priority)
  - [Component Source File](#component-source-file)
    - [File Anatomy](#file-anatomy)
    - [Runtime State](#runtime-state)
  - [Canvas Rendering](#canvas-rendering)
    - [ComponentItem Subclass](#componentitem-subclass)
    - [emitInitialValue()](#emitinitialvalue)
    - [Input Handling](#input-handling)
  - [Wiring to the Runtime](#wiring-to-the-runtime)
  - [Board Profile Considerations](#board-profile-considerations)
  - [Testing a New Component](#testing-a-new-component)
  - [Worked Example](#worked-example)
  - [Checklist](#checklist)

## Registering a Component
There's no `ComponentType` enum, that used to exist and was removed in favor of a flat registry, specifically so adding a component never means touching a shared switch statement somewhere else in the codebase. A component is just a `ComponentDefinition` struct, registered via a static-init lambda at the bottom of its own `.cpp` file:
```c++
struct ComponentDefinition {
    std::string type_name;
    std::vector<std::string> detect_single;
    std::vector<PinRole> detect_multi;
    std::vector<std::string> detect_pattern;
    bool is_output;
    std::function<ComponentItem*(int pin, QGraphicsItem*)> create_item;
    MultiPinStrategy multi_pin_strategy = MultiPinStrategy::None;
    std::string representative_role;
    QColor wire_color = QColor("#888888");

    // Escape hatch for detection syntax the generic engine can't express.
    std::function<void(CircuitDetector& ctx, const std::string& source,
                        const std::map<std::string, std::string>& defines,
                        const std::map<std::string, std::vector<int>>& arrays,
                        std::set<int>& claimed)> detect_custom;
};
```
```c++
// src/components/buzzer.cpp
static bool registered = []() {
    ComponentDefinition def{
        "Buzzer",
        {"BUZZER", "BUZZ", "SPEAKER", "TONE", "PIEZO"},
        {},    // detect_multi -- none, this is a single-pin component
        {},    // detect_pattern -- none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new BuzzerItem(pin, parent);
        }
    };
    def.wire_color = BUZZER_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
```
`ComponentRegistry::instance().all()` is what `CircuitDetector` and `CanvasWidget` iterate, so this one registration is the only place a new component's detection rules and its canvas factory need to live.

## Detection
### Keyword Matching
`detect_single` is the tier-3 fallback: a list of uppercase substrings checked against every pin name the sketch actually uses (from a `#define`, `const int`, or `pinMode()`/`analogRead()` call) that no earlier tier already claimed. `ComponentRegistry::find_by_single_keyword()` does a longest-keyword-wins substring match, so if two components' keyword lists could both match a name, the longer keyword takes it.

### Multi-Pin Groups
`detect_multi` is a list of `PinRole{name, keywords}` for components that need more than one pin. `multi_pin_strategy` picks how those roles get resolved:
- **`Suffix`**: strips each role's matched keyword off the pin name and groups roles that share the same leftover suffix. `TRIG_PIN`/`ECHO_PIN` both strip down to `_PIN`, so they're grouped together.
- **`Prefix`**: groups pin names by whatever comes before their first underscore, then assigns each role its keyword-matched pin within that group. `MOTOR_PWM`/`MOTOR_DIR` group under `MOTOR`.
- **`Array`**: each role is a whole array variable (`const int S2[] = {A2};`), and roles are zipped by index, so parallel arrays of equal length can describe multiple component instances at once.
- **`Singleton`**: no shared naming convention required at all, every `#define` in the sketch is checked against every role's keywords, first match per role wins. This is the loosest strategy, use it when there's no sensible prefix/suffix relationship between the pins (e.g. bare `OUT`/`S2`/`S3` defines for a color sensor).

`representative_role` picks which role's resolved pin becomes `comp.pin`, the one used for the component's primary lookup and label; it defaults to the first role if left blank.

### Paired Defines
The common two-pin case, `TRIG`/`ECHO`, `RS`/`EN`, etc., is just the `Suffix` strategy in practice. Real example from `distance_sensor.cpp`:
```c++
ComponentDefinition def{
    "DistanceSensor",
    {"TRIG", "ECHO", "DISTANCE", "ULTRASONIC", "SONAR", "HCSR"},
    {
        {"TRIG", {"TRIG"}},
        {"ECHO", {"ECHO"}},
    },
    {"pulseIn("},
    false,
    [](int pin, QGraphicsItem* parent) -> ComponentItem* {
        return new DistanceSensorItem(pin, parent);
    },
    MultiPinStrategy::Suffix,
    "ECHO"
};
```
A sketch with `#define TRIG_PIN 9` and `#define ECHO_PIN 10` gets grouped because both strip down to the same `_PIN` suffix, and `ECHO` becomes the component's primary pin since that's the `representative_role`.

### Custom Detection
When a component's constructor or naming shape doesn't fit any `MultiPinStrategy`, set `detect_custom` instead of (or alongside) `detect_multi`: a lambda that gets the raw source, the parsed `#define`s/arrays, and the `claimed` set, and calls `ctx.add_detected_component(comp, claimed)` directly. This is how Keypad, DHT, MAX7219/LedControl, NeoPixel, the OLED, and the I2C LCD variant handle detection, cases with independent row/col counts, a non-pin constructor argument, positional-literal constructor args, or (for I2C devices) no dedicated GPIO pin at all, they key off a synthetic bus pin from `ctx.next_i2c_bus_pin()` instead. Real example, `lcd.cpp`'s `LiquidCrystal_I2C` handling:
```c++
def.detect_custom = [](CircuitDetector& ctx, const std::string& source,
                        const std::map<std::string, std::string>& defines,
                        const std::map<std::string, std::vector<int>>&,
                        std::set<int>& claimed) {
    static const std::regex ctor_re(
        R"(\bLiquidCrystal_I2C\s+(\w+)\s*(?:=\s*LiquidCrystal_I2C\s*)?\(\s*(\w+)\s*(?:,\s*(\w+)\s*,\s*(\w+)\s*)?\))");
    for (auto it = std::sregex_iterator(source.begin(), source.end(), ctor_re);
         it != std::sregex_iterator(); ++it) {
        // ... resolve cols/rows, build a DetectedComponent, ...
        int pin_key = ctx.next_i2c_bus_pin();
        ctx.add_detected_component(comp, claimed);
    }
};
```

### Detection Priority
`CircuitDetector::detect()` runs, in this exact order, and a pin claimed by an earlier step is skipped by every later one:
1. **Multi-pin groups** (`detect_generic_multipin`), all four strategies above.
2. **Pattern matches** (`detect_pattern_matches`): a `.method(`-style call (e.g. `myServo.attach(9)`), a wrapper-function call (e.g. anything that calls `pulseIn(...)`), or a constructor call (e.g. `LiquidCrystal lcd(8,9,...)`), depending on the shape of the pattern string in `detect_pattern`.
3. **`detect_custom` callbacks**: Keypad matrices, `DHT` sensors, `LedControl`/MAX7219 chains, NeoPixel, the OLED, and the I2C LCD, see [Custom Detection](#custom-detection) above; these live in each component's own `.cpp` via the `detect_custom` field rather than in `circuitdetector.cpp`.
4. **Single-keyword fallback**, from whatever `pinMode()`/`analogRead()` calls are left.

If two components would still end up claiming the same pin, the first one to claim it wins and a warning is surfaced rather than the second one silently overwriting it.

## Component Source File
### File Anatomy
Every component is one `.cpp` file under `src/components/`, no header needed. The shape is always the same: a `ComponentItem` subclass, then a static-init lambda that builds a `ComponentDefinition` and registers it.
```c++
#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

class MyComponentItem : public ComponentItem {
    // ... state, boundingRect(), paint(), input handlers ...
};

static bool registered = []() {
    ComponentDefinition def{ /* ... */ };
    ComponentRegistry::instance().register_component(def);
    return true;
}();
```
CMake's glob picks up any new file here automatically, there's nothing else to add it to.

### Runtime State
If your component needs new state in `RuntimeState` (`arduinoruntime.h`), most per-pin fields there are fixed-size C arrays (`pin_values[80]`, `analog_values[20]`, etc., see [Board Profile Considerations](#board-profile-considerations)), so a new per-pin value usually just needs a `std::map<int, T>` alongside a small mutex if it's written from the GUI thread and read from the sketch thread, following the pattern `color_channels_`/`color_mtx_` or `dht_readings_`/`dht_mtx_` already use. You'll also need a matching `inject_*()` method on `ArduinoRuntime` and a `SketchThread::inject*()`/`SketchHost::inject_*()` passthrough, see [Wiring to the Runtime](#wiring-to-the-runtime).

## Canvas Rendering
### ComponentItem Subclass
Every component overrides at minimum the two pure-virtuals:
```c++
QRectF boundingRect() const override;
void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
```
`CanvasWidget` never switches on component type to decide size or drawing, it just calls these on whatever `ComponentItem` your `create_item` factory returned. Everything else is optional overrides, only implement what your component actually needs:
```c++
virtual void onPinChanged(int pin, int value);       // sketch wrote to this pin -- update visual state
virtual void updateText(int row, const QString&);     // LCD text updates
virtual void updateMatrixRow(int addr, int row, int bits); // MAX7219 row updates
virtual void configureMultiPin(const std::vector<int>& pins); // multi-pin components: all resolved pins
virtual void configureRowsCols(int rows, int cols);    // Keypad only
virtual void configureDeviceCount(int count);          // MAX7219 only, affects boundingRect() sizing
virtual bool supportsRotation() const;                  // CTRL+click config: rotation
virtual bool supportsColorConfig() const;               // CTRL+click config: base color
virtual bool supportsPolarity() const;                  // CTRL+click config: common cathode/anode
```

### emitInitialValue()
```c++
virtual void emitInitialValue();
```
`CanvasWidget` calls this once, right after connecting your item's `inputChanged` signal. If your component has a value to push at startup (a switch's current position, a text field's default reading), emit it here, **not** from the constructor or from `configureMultiPin()`. Both of those run before the caller has anything connected yet, so a signal emitted there is just silently dropped, not queued.

### Input Handling
Interactive components override Qt's own mouse events directly on the graphics item (`mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent`) and emit the one shared signal:
```c++
Q_SIGNALS:
    void inputChanged(int pin, int eventType, QVariant value);
```
`eventType` is a `ComponentEventType`:
```c++
enum class ComponentEventType {
    DigitalPress, BouncePress, AnalogValue, PulseUs,
    ColorRGB, KeypadWiring, KeypadPress, DhtReading,
};
```
Real example, a toggle switch:
```c++
void mousePressEvent(QGraphicsSceneMouseEvent*) override {
    switch_ = !switch_;
    update();
    emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, switch_ ? 1 : 0);
}

void emitInitialValue() override {
    emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, switch_ ? 1 : 0);
}
```
Use `DigitalPress` for a clean on/off (no bounce), `BouncePress` for a component that should get the ~10ms button-bounce simulation, and `AnalogValue`/`PulseUs`/`ColorRGB` for the matching non-boolean inputs. Multi-value payloads (`ColorRGB`, `KeypadWiring`, `KeypadPress`, `DhtReading`) pack their values into a `QVariantList`.

## Wiring to the Runtime
`CanvasWidget::onComponentInput()` is the one place that dispatches `ComponentEventType` to a `SketchThread::inject*()` call:
```c++
switch (static_cast<ComponentEventType>(eventType)) {
    case ComponentEventType::DigitalPress:  sketchThread_->injectPin(pin, value.toInt()); break;
    case ComponentEventType::BouncePress:   sketchThread_->injectButtonBounce(pin, value.toInt()); break;
    case ComponentEventType::AnalogValue:   sketchThread_->injectAnalog(pin, value.toInt()); break;
    case ComponentEventType::PulseUs:       sketchThread_->injectPulseDuration(pin, value.toULongLong()); break;
    // ColorRGB, KeypadWiring, KeypadPress, DhtReading unpack a QVariantList first
}
```
If your component fits one of the existing event types (most do), there's nothing else to wire, `injectPin`/`injectAnalog`/etc. already reach `ArduinoRuntime` through `SketchHost`. If it genuinely needs a new kind of value (like `ColorRGB`, `KeypadWiring`, and `DhtReading` did), that's four small additions: a new `ComponentEventType`, a case in `onComponentInput()`, an `inject*()` method on `SketchThread`/`SketchHost` forwarding to `ArduinoRuntime`, and the actual `RuntimeState` field it lands in.

## Board Profile Considerations
Most per-pin `RuntimeState` arrays are fixed at 80 slots (`pin_modes`, `pin_values`, `pwm_values`, `pulse_durations_`, `pin_driven`, `pin_bounce_target`), and `analog_values` at 20. Every current board profile's `pin_count`/`analog_count` fits comfortably under those (Mega, the largest, is 70 pins/16 analog), so this is a non-issue for now, but a component that needs to address a pin number directly should stay aware these arrays aren't dynamically sized. `AVR GPIO`/`AVR Timer` register access (if your component's real-world counterpart is often driven through `DDRx`/`OCRx` instead of `pinMode`/`analogWrite`) is Uno/Nano-shaped only, ports B/C/D and Timer1/Timer2, regardless of the selected board, see [API_REFERENCE.md](API_REFERENCE.md).

## Testing a New Component
The fastest way to test a new component end-to-end is a `.timeline` fixture, same pattern the rest of the test suite uses:
1. Drop a sketch at `app/sketches/tests/<YourComponent>/<YourComponent>.cpp` that exercises the component.
2. Add a sibling `<YourComponent>.timeline` that injects stimulus and asserts on the result, e.g.:
   ```
   0.5, SET, MYSENSOR, 512
   1,   ASSERT, PIN, LED1, HIGH
   ```
3. Run it directly: `./app/VEMCODE app/sketches/tests/<YourComponent>/<YourComponent>.cpp timeline=true speed=10`
4. Or run the whole suite: `./scripts/run_tests.sh`

See [ARCHITECTURE.md](ARCHITECTURE.md#headless-mode) for the full `.timeline` format and available ASSERT/action targets.

## Worked Example
Say you want to add a **Relay** module, a single-pin digital output, functionally almost identical to an LED but with its own label and color.

1. **Create `src/components/relay.cpp`** with a minimal `ComponentItem`:
   ```c++
   #include "src/core/circuit/componentitem.h"
   #include "src/core/circuit/componentregistry.h"
   #include <QPainter>

   static const QColor RELAY_ACTIVE  ("#dc7474");
   static const QColor RELAY_INACTIVE("#371010");

   class RelayItem : public ComponentItem {
       bool active_ = false;
   public:
       RelayItem(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {}

       QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

       void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
           p->setBrush(active_ ? RELAY_ACTIVE : RELAY_INACTIVE);
           p->drawRect(boundingRect());
           p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "Relay");
       }

       void onPinChanged(int, int value) override {
           active_ = (value > 0);
           update();
       }
   };
   ```
2. **Register it**, picking keywords that won't collide with an existing component (`RELAY` isn't taken):
   ```c++
   static bool registered = []() {
       ComponentDefinition def{
           "Relay",
           {"RELAY", "RLY"},
           {}, {}, true,
           [](int pin, QGraphicsItem* parent) -> ComponentItem* {
               return new RelayItem(pin, parent);
           }
       };
       def.wire_color = RELAY_ACTIVE;
       ComponentRegistry::instance().register_component(def);
       return true;
   }();
   ```
3. **Build and test it** with a sketch: `#define RELAY_PIN 7` + `pinMode(RELAY_PIN, OUTPUT)` + `digitalWrite(RELAY_PIN, HIGH)` should show up on the canvas immediately, no other file needed changing.
4. Since it's output-only and single-pin, there's no `emitInitialValue()`/`inputChanged` to write, `onPinChanged()` is the whole behavior.
5. Add a `.timeline` fixture per [Testing a New Component](#testing-a-new-component) to lock the behavior in.

## Checklist
- [ ] New `.cpp` file under `src/components/`, no header needed.
- [ ] `ComponentItem` subclass implementing `boundingRect()`/`paint()`, plus `onPinChanged()`/input handlers/`emitInitialValue()` as needed.
- [ ] `ComponentDefinition` registered via a static-init lambda, with `detect_single`/`detect_multi`/`detect_pattern` keywords that don't collide with an existing component.
- [ ] If interactive: emits `inputChanged()` with an existing `ComponentEventType`, or a new one wired through `onComponentInput()` → `SketchThread` → `ArduinoRuntime`.
- [ ] A `.timeline` test fixture under `app/sketches/tests/`.
- [ ] Add it to the Supported Components list in [README.md](../README.md) and the keyword table in [SKETCH_GUIDE.md](SKETCH_GUIDE.md) and [API_REFERENCE.md](API_REFERENCE.md).
