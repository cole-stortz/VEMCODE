# VEMCODE : API Reference

**Overview:**
This is a reference for what Arduino API surface VEMCODE actually implements: exact signatures, valid ranges, and where the simulation quietly diverges from real hardware. It's not a tutorial, see [SKETCH_GUIDE.md](SKETCH_GUIDE.md) for that, this doc is for looking up "does X work, and what exactly does it do here." A condensed version of the core entries here is also available by right-clicking a function directly in the editor.

## Table of Contents
- [VEMCODE : API Reference](#vemcode--api-reference)
  - [Table of Contents](#table-of-contents)
  - [Component Detection](#component-detection)
    - [How VEMCODE Detects Components](#how-vemcode-detects-components)
    - [Keyword Table](#keyword-table)
  - [Digital I/O](#digital-io)
  - [Analog I/O](#analog-io)
  - [Timing](#timing)
  - [Math and Utilities](#math-and-utilities)
  - [Tone](#tone)
  - [pulseIn](#pulsein)
  - [Serial](#serial)
  - [Wire / I2C](#wire--i2c)
  - [SPI](#spi)
  - [AVR GPIO Registers](#avr-gpio-registers)
  - [AVR Timer Registers](#avr-timer-registers)
  - [Interrupts](#interrupts)
    - [attachInterrupt](#attachinterrupt)
    - [ISR() Vector Macros](#isr-vector-macros)
    - [noInterrupts and interrupts](#nointerrupts-and-interrupts)
  - [Supported Libraries](#supported-libraries)
    - [Servo](#servo)
    - [SoftwareSerial](#softwareserial)
    - [EEPROM](#eeprom)
    - [avr/wdt.h](#avrwdth)
    - [LiquidCrystal](#liquidcrystal)
  - [AVR Compatibility](#avr-compatibility)
    - [PROGMEM and F() Macro](#progmem-and-f-macro)
    - [pgm\_read\_\* Functions](#pgm_read_-functions)
    - [AVR Assembly](#avr-assembly)
    - [#ifdef ARDUINO](#ifdef-arduino)
    - [util/delay.h](#utildelayh)

## Component Detection
### How VEMCODE Detects Components
The circuit detector reads your sketch's `#define`s, `const int`s, `pinMode()` calls, and constructor/method calls to figure out what's wired to each pin. Detection runs in tiers, and once a pin is claimed by an earlier tier, later tiers skip it:
- **Pattern matching**: a constructor or method call shape, e.g. `myServo.attach(9)` or `LiquidCrystal lcd(8,9,10,11,12,13)`.
- **Multi-pin roles**: paired/grouped defines that share a prefix or suffix, e.g. `TRIG_PIN`/`ECHO_PIN` for a distance sensor.
- **Keyword fallback**: any remaining `pinMode()`/`analogRead()` pin gets matched against a single-keyword table (below); unmatched pins become a generic input/output.
- **Special cases**: Keypad matrices, `DHT` sensors, and `LedControl`/MAX7219 chains are detected by their own hand-written patterns rather than the tables above.

If two components would claim the same pin, the earlier tier wins and a warning is printed rather than silently overwriting the first match.

### Keyword Table
Keywords are matched case-insensitively as a substring of your pin's name (from a `#define`, `const int`, or variable name). Multi-pin components need one keyword per role (e.g. a distance sensor needs both a `TRIG` and an `ECHO` pin); everything else just needs one matching pin name.

| Component | Single-pin keywords | Multi-pin roles |
| --- | --- | --- |
| LED | `LED`, `LAMP`, `DIODE`, `INDICATOR` | |
| Button (bouncy) | `BUTTON`, `BTN`, `TACT`, `PUSH`, `KEY` | |
| Button (clean, no bounce) | `CLEAN`, `IDEAL` | |
| Switch | `TOGGLE`, `SWITCH` | |
| Buzzer | `BUZZER`, `BUZZ`, `SPEAKER`, `TONE`, `PIEZO` | |
| Servo | `SERVO`, `SRV` | detected via `.attach(` instead |
| H-Bridge Motor | `MOTOR`, `HBRIDGE`, `ENA`, `IN1` | `PWM`/`ENA`, `CWISE`/`IN1` (`CW`, `DIR`), `ANTI_CWISE`/`IN2` (`ANTI`) |
| Potentiometer | `POT`, `POTENTIOMETER`, `KNOB`, `DIAL` | |
| Light Sensor | `LIGHT`, `LDR`, `PHOTO` | |
| Temperature Sensor | `TEMP`, `TEMPERATURE`, `THERMISTOR`, `NTC` | |
| Force Sensor | `FORCE`, `FORCESENSOR`, `FORCE_SENSOR` | |
| Generic Analog Sensor | `SENSOR`, `ANALOG`, `ADC` | |
| Distance Sensor | `TRIG`, `ECHO`, `DISTANCE`, `ULTRASONIC`, `SONAR`, `HCSR` | `TRIG`, `ECHO` (suffix-paired) |
| Joystick | `JOYSTICK`, `JOY`, `VRX`, `VRY` | `VRX`, `VRY`, `SW` (suffix-paired) |
| Color Sensor (TCS3200-style) | `COLOR`, `TCS`, `S2`, `S3` | `OUT` (or `SENSOROUT`), `S2`, `S3` |
| Rotary Encoder | `ENCODER`, `ROTARY`, `CLK`, `DT` | `CLK`, `DT` (suffix-paired) |
| Stepper | `STEPPER`, `STEP`, `DIR` | `STEP`+`DIR`, or bare `IN1`-`IN4` |
| Keypad | `KEYPAD` | row/col pin arrays, see below |
| DHT (temp/humidity) | `DHT`, `DHTPIN`, `DHT_PIN` | detected via `DHT name(pin, type)` constructor |
| MAX7219 / LedControl | | `CS`, `CLK`, `DIN` (bare or prefixed), or via `LedControl` constructor |
| Seven-Segment Display | | `SEG_A`..`SEG_G` (or `SEGA`..`SEGG`) |
| RGB LED | | `REDPIN`/`R_PIN`, `GREENPIN`/`G_PIN`, `BLUEPIN`/`B_PIN` (suffix-paired) |
| LCD | `LCD`, `DISPLAY`, `SCREEN`, `OLED` | `RS`, `EN`, `D4`-`D7`, or via `LiquidCrystal` constructor |
| IR Sensor | `IR`, `IRSENSOR`, `IR_SENSOR`, `IR_OUT`, `INFRARED` | |

- **Keypad**: detected from a `byte`/`int`/`uint8_t` array named with `ROW`/`COL` in it (`byte rowPins[4] = {9,8,7,6};`), or from numbered defines like `ROW1..ROW4`/`COL1..COL4`. Needs 2-4 rows and 2-4 columns.
- **DHT**: detected from a `DHT name(PIN, TYPE);` constructor call, the type argument (e.g. `DHT11`/`DHT22`) is just used for the label.
- **MAX7219**: detected from `LedControl lc(dataPin, clkPin, csPin[, numDevices]);`; device count is clamped 1-8.

## Digital I/O
- `pinMode(pin, mode)`: `mode` is `INPUT`, `OUTPUT`, or `INPUT_PULLUP`. `INPUT_PULLUP` immediately reads HIGH until something drives it low, matching a real pull-up's idle state.
- `digitalWrite(pin, value)`: only fires the canvas/interrupt/ISR machinery when the value actually changes; writing the same value twice is a no-op.
- `digitalRead(pin)` has three special cases, checked in this order:
	- A button mid-bounce (~10ms after a click) returns noise before settling.
	- A keypad matrix column pin resolves from which row is currently driven and which key is held.
	- An `INPUT` pin with nothing wired to it (no UI component, no injected value) returns random noise every read, this never stops just because you read it once.
- Out-of-range pin numbers are silently ignored on all three functions, no error, no warning.

## Analog I/O
- `analogWrite(pin, value)`: **not clamped** to 0-255 (or any board's PWM resolution) and **not validated** against which pins actually support PWM. The board's PWM-capable pin list is only used for a static lint warning ("won't have any effect on real hardware"), the simulation applies the write regardless.
- `analogRead(pin)`: accepts either the raw `A0`-style pin constant (`A0`=14, etc.) or a bare analog channel index; out-of-range returns `0`.
	- Optional Gaussian sensor noise (σ=2, toggled in Settings) is added and clamped to 0-1023, even on boards with a 12-bit ADC (Due/Teensy) it still clamps to the 10-bit range.

## Timing
- `delay(ms)` and `millis()`/`micros()` all scale by the speed slider (1x-25x → 0.1x-2.5x wall-clock).
- `delay()` sleeps in 10ms chunks so interrupts/ISRs can still preempt it and so Stop responds quickly.
- `delayMicroseconds(us)` is a busy-poll loop, not a hardware timer, so it's not microsecond-precise; treat it as approximate.
- `millis()`/`micros()` overflow the same way real `unsigned long` values do; no special wraparound handling.

## Math and Utilities
| Function | Notes |
| --- | --- |
| `map(x, in_min, in_max, out_min, out_max)` | Returns `out_min` if `in_max == in_min` instead of dividing by zero (real AVR macro would crash here). |
| `constrain(x, lo, hi)` | Standard clamp. |
| `abs(x)`, `min(a,b)`, `max(a,b)` | Macros, same as real Arduino, so an argument with a side effect (`max(i++, j)`) can evaluate twice. |
| `random(max)` / `random(min, max)` | Thin wrapper over C's `rand()`; `random(0)` is undefined behavior, same footgun as the real macro. |
| `randomSeed(seed)` | `srand()` under the hood. |
| `analogReference(mode)` | No-op; simulation ignores ADC reference voltage entirely. |
| `pow`, `sqrt` | Unmodified, whatever your included headers provide. |

## Tone
- `tone(pin, frequency, duration=0)`: frequency/duration aren't validated or clamped. `duration=0` plays until you call `noTone()`, matching real Arduino.
- `noTone(pin)`: silences immediately, but if a `tone()` call's own duration timer is still pending, it'll still harmlessly zero the pin again when it elapses.

## pulseIn
`pulseIn(pin, value, timeout=1000000UL)` (timeout in µs, same default as real Arduino) resolves in one of three ways:
- If a UI component injected a fixed pulse duration (e.g. a Distance Sensor's cm input), that value returns immediately.
- If the pin belongs to a Color Sensor, it returns a period computed from the sensor's current color reading.
- Otherwise it's a real polling loop (wait idle → wait for pulse start → measure), returning `0` on timeout.

## Serial
- `Serial.begin(baud)`: `baud` is stored but has no effect on timing simulation, any value "works."
- `Serial.print`/`println` support the usual `HEX`/`DEC`/`OCT`/`BIN` and float-decimals overloads.
- `Serial.available()`/`.read()` return `0`/`-1` when there's nothing to read, same as real hardware.
- **`Serial1`/`Serial2`** only support `.begin()`/`.print()`/`.println()`, calling `.available()`, `.read()`, or `.peek()` on them will fail to compile.
- `SoftwareSerial` has its own full API, see [Supported Libraries](#supported-libraries).

## Wire / I2C
- `Wire.begin()` / `Wire.begin(address)`: no-op; **no slave mode**, `onReceive`/`onRequest` don't exist.
- `Wire.beginTransmission(address)`, `.write(...)` (byte/string/buffer overloads), `.endTransmission()`: always reports success, no bus errors are modeled.
- `Wire.requestFrom(address, quantity)`: returns bytes from the Debug Panel's virtual I2C device panel for that address; missing bytes are zero-padded rather than erroring, and an unconfigured address just returns all zeros.
- `Wire.available()` / `.read()`: standard buffer semantics, `-1` when empty.

## SPI
- `SPISettings(clock, bitOrder, dataMode)`: all three arguments are accepted (for compile compatibility) and then ignored.
- `SPI.begin()` / `.beginTransaction()` / `.endTransaction()`: no-ops.
- `SPI.transfer(byte)`: the byte you send is ignored; it returns the next byte from the Debug Panel's virtual SPI byte sequence, wrapping around when it runs out.
- `SPI.transfer(buf, len)`: overwrites the buffer in place, one virtual byte per position.
- No per-device chip-select handling, same as real SPI: toggle your own CS pin with `digitalWrite()`.

## AVR GPIO Registers
`DDRx`/`PORTx`/`PINx` are available for **ports B, C, and D only** (the Uno/Nano pin mapping), always, regardless of which board is selected in Settings, even Mega, Due, and Teensy profiles still get the Uno mapping rather than their real port layout.
- `PORTB`/`DDRB`/`PINB` → pins 8-13.
- `PORTC`/`DDRC`/`PINC` → pins 14-19 (A0-A5).
- `PORTD`/`DDRD`/`PIND` → pins 0-7.
- Writing `PINx` toggles the matching `PORTx` bits rather than setting them, this is a real, intentionally-preserved AVR quirk, not a bug.

## AVR Timer Registers
Timer1 (16-bit) and Timer2 (8-bit) registers are supported: `TCCR1A/B`, `TCNT1`, `OCR1A` (pin 9), `OCR1B` (pin 10), `TIMSK1`, and the Timer2 equivalents `OCR2A` (pin 11), `OCR2B` (pin 3).
- **Timer0 doesn't exist** as a register in VEMCODE. `millis()`/`delay()` are wall-clock based and completely unaffected by any timer register writes, so "freeing up Timer0" tricks have nothing to reconfigure.
- `F_CPU` is fixed at 16MHz for all timer math, regardless of the selected board.
- Writing `OCRxA`/`OCRxB` also directly drives that pin's `analogWrite()` duty cycle (clamped 0-255), in addition to updating the register value used for interrupt timing.
- Interrupt firing is polled at ~1ms real-time resolution, so very high-frequency timer interrupts won't be cycle-accurate.

## Interrupts
### attachInterrupt
`attachInterrupt(pin, callback, mode)` accepts **any pin**, VEMCODE doesn't enforce the real Uno's pin-2/3-only restriction. `digitalPinToInterrupt(pin)` is just the identity function. Modes: `CHANGE`, `RISING`, `FALLING`.
- `detachInterrupt()` **is not implemented**, calling it will fail to compile.

### ISR() Vector Macros
`ISR(vector_name)` works for these 13 vectors; anything else compiles but its handler will simply never fire:

| | | |
| --- | --- | --- |
| `INT0_vect` | `INT1_vect` | `PCINT0_vect` |
| `PCINT1_vect` | `PCINT2_vect` | `WDT_vect` |
| `USART_RX_vect` | `TIMER1_OVF_vect` | `TIMER1_COMPA_vect` |
| `TIMER1_COMPB_vect` | `TIMER2_OVF_vect` | `TIMER2_COMPA_vect` |
| `TIMER2_COMPB_vect` | | |

Known quirk: `TIMER2_COMPA_vect` and `TIMER2_COMPB_vect` do actually fire, but the compiler's "is this vector simulated" check doesn't know about them yet, so you may see a spurious "not simulated" warning for those two specifically, safe to ignore.

### noInterrupts and interrupts
A single global flag gates all interrupt/ISR dispatch, `attachInterrupt` callbacks, `ISR()` vectors, and the watchdog/timer ISRs alike. There's no per-vector masking beyond your sketch's own `TIMSK`/`PCICR` writes.

## Supported Libraries
### Servo
- `attach(pin)`: one-argument form only, no `attach(pin, min, max)`.
- `write(angle)`: remaps 0-180 to a 0-255 PWM value, this is not a true microsecond pulse-width simulation.
- `read()`, `attached()`, `detach()` are supported. `writeMicroseconds()` is not.

### SoftwareSerial
- `SoftwareSerial(rxPin, txPin)`, `begin(baud)`, `print`/`println` (const char*, String, int, long, unsigned long, float), `available()`, `read()`, `peek()`, `write()`.
- Output shows up in the main Serial Monitor prefixed `[SW:N]` (N = the RX pin).
- `listen()`/`isListening()` always return `true`, `overflow()` always returns `false`, multi-instance RX contention isn't modeled.

### EEPROM
- `EEPROM.read(address)`, `.write(address, value)`, `.update(address, value)` over a 1024-byte space.
- Out-of-range writes/updates are silently ignored; out-of-range reads return `0xFF`, matching real AVR behavior.
- Nothing persists between runs.

### avr/wdt.h
- `wdt_enable(timeout_ms)` takes a **raw millisecond integer**, not the real AVR's register-bit enum.
- The `WDTO_15MS`...`WDTO_8S` constants are provided for compile compatibility, but are defined as plain millisecond values (e.g. `WDTO_2S` = `2000`), not the real hardware's bit-pattern values, don't do bitwise math on them expecting AVR semantics.
- `wdt_reset()`, `wdt_disable()` work as expected.

### LiquidCrystal
- One constructor: `LiquidCrystal(rs, en, d4, d5, d6, d7)`. Other real-Arduino overloads (8-bit mode, with an RW pin) aren't supported.
- `begin(cols, rows)`, `clear()`, `setCursor(col, row)`, `print(...)`, `write(byte)`.
- Display is always modeled as 16x2 internally regardless of the `cols`/`rows` you pass to `begin()`, larger displays (20x4, etc.) aren't actually simulated at that size.
- `createChar()` compiles and accepts data but doesn't render a custom glyph, it's a no-op.

## AVR Compatibility
### PROGMEM and F() Macro
Both are no-ops (`#define PROGMEM` / `#define F(x) (x)`), there's no separate flash/RAM distinction in simulation, so anything marked `PROGMEM` just lives in normal memory.

### pgm_read_* Functions
`pgm_read_byte`/`_word`/`_dword`/`_float` are plain pointer casts and dereferences, since `PROGMEM` data isn't actually anywhere special to read from.

### AVR Assembly
Only inline `asm`/`__asm__` blocks containing a single recognized instruction are transformed; anything else is stripped with a warning rather than failing the build.

| Instruction | Becomes |
| --- | --- |
| `cli` | `noInterrupts()` |
| `sei` | `interrupts()` |
| `nop` | removed silently |
| `sleep`, `wdr`, `rjmp 0` | removed, with a warning that there's no simulation equivalent |
| anything else | removed, with a "no simulation equivalent" warning |

### #ifdef ARDUINO
`ARDUINO` is defined (as `100`), so `#ifdef ARDUINO`/`#if defined(ARDUINO)` guards take the Arduino branch and compile normally.

### util/delay.h
`_delay_ms()`/`_delay_us()` keep working even without this include, they're mapped straight to `delay()`/`delayMicroseconds()`. `F_CPU` is fixed at 16MHz regardless of the selected board.
