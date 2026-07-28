# VEMCODE : Sketch Guide

**Overview:**
This guide covers how to write sketches for VEMCODE. If you have Arduino/Teensy experience, most of what this document will reference will be familiar to you. This is not a full Arduino programming tutorial, there are much better resources for that, but this document covers whats supported, what is not, and what to look out for for common errors and warnings. 

---
## Sketch Structure

### setup() and loop()
Every Sketch for the supported boards need to include this basic structure for programming embedded sketches:
```c++
void setup() {
	/* 
	Runs once before anything else to initialize pins and any other
	funciton that you want to run at boot. 
		- EX: pinMode(COMP_NAME, INPUT);
	*/
}

void loop() {
	/* 
	Function runs continuously in a loop like a conventional loop. Will only 
	stop after stopping the sketch or returning from the funciton to conclude 
	all funcitons.
		- EX: while(true) {} 
    */
	
	// If a delay is ommited, VEMCODE injects a safety delay to fix 
	// issue of crashing from overloading the process.
	delay(1); 
}
```
### Selecting a Board
This could be accomplished through one of two ways, either you go through the settings menu where you can choose a board to simulate in a drop down menu, or can override that step though a initial comment in the sketch:
- EX: `// @board Arduino Uno`, `// @board Teensy 4.1`, etc.

---
## Pin and Component Naming

### How VEMCODE Detects Components
The Components are detected through a system of reading through the `#defines`, `const int`, `pinMode`, and other calls to infer what the component is which means that how you define your pins matters. Some examples of component definitions that would work in VEMCODE:
- `#define MY_LED_PIN 9` >> detects a LED at pin 9
- `const int BTN = 14`     >> detects a button at pin 14
- `pinMode(LED, OUTPUT)` >> detects a LED as an output
- `myServo.attach(9);`     >> detects a servo at pin 9, labeled `myServo`
- `LiquidCrystal lcd(8, 9, 10, 11, 12, 13)` >> detects a LCD at specified pins
### Naming Conventions That Work
VEMCODE matches keywords against your pin names (case-insensitive). Any pin name containing one of these words will be detected as that component:

| Component | Keywords | Multi-pin roles |
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
| NeoPixel / WS2812B | `NEOPIXEL`, `WS2812`, `PIXEL`, `PIXELS`, `STRIP` | detected via `Adafruit_NeoPixel` constructor instead |
| Seven-Segment Display | | `SEG_A`..`SEG_G` (or `SEGA`..`SEGG`) |
| RGB LED | | `REDPIN`/`R_PIN`, `GREENPIN`/`G_PIN`, `BLUEPIN`/`B_PIN` (suffix-paired) |
| LCD | `LCD`, `DISPLAY`, `SCREEN`, `OLED` | `RS`, `EN`, `D4`-`D7`, or via `LiquidCrystal` constructor |
| IR Sensor | `IR`, `IRSENSOR`, `IR_SENSOR`, `IR_OUT`, `INFRARED` | |

- **Keypad**: detected from a `byte`/`int`/`uint8_t` array named with `ROW`/`COL` in it (`byte rowPins[4] = {9,8,7,6};`), or from numbered defines like `ROW1..ROW4`/`COL1..COL4`. Needs 2-4 rows and 2-4 columns.
- **DHT**: detected from a `DHT name(PIN, TYPE);` constructor call.
- **MAX7219**: detected from `LedControl lc(dataPin, clkPin, csPin[, numDevices]);`.
- **NeoPixel**: detected from `Adafruit_NeoPixel strip(count, pin[, type]);`.

If a pin name matches more than one component's keywords, the longest matching keyword wins.
### Hardcoded Pin Numbers
When initializing pins in VEMCODE, you cannot just use base ints like you can in arduino:
```c++
// hardcoded - VEMCODE has no name to match keywords against
digitalWrite(5, HIGH);
analogRead(14);
```
Hardcoded pins will compile and run but wont appear on the canvas, the fix is to just give pins appropriate names:
```c++
// named - VEMCODE sees "LED_PIN" and detects an LED
#define LED_PIN 5
digitalWrite(LED_PIN, HIGH);
```
### Pins Defined as Expressions
The circuit detector does not have the ability to handle functions set in the pin names, the code compiles and runs fine but the circuit detector will have issues detecting the component at that pin.
```c++
// expression - VEMCODE has no pin to match keywords against
#define LED_PIN 1+2 
```

---
## Supported Arduino API

### Digital I/O
- `pinMode(pin, mode)` - sets a pin as `INPUT`, `OUTPUT`, or `INPUT_PULLUP`
- `digitalWrite(pin, value)` - sets a pin `HIGH` or `LOW`; fires the signal timeline and dispatches any registered interrupt handlers on state change
- `digitalRead(pin)` - returns the current pin state; returns random `HIGH`/`LOW` on floating `INPUT` pins to simulate real hardware noise
### Analog I/O
- `analogRead(pin)` - returns 0–1023; supports `A0`–`A7` notation; optional gaussian noise can be enabled in Settings
- `analogWrite(pin, value)` - writes a PWM value; not actually clamped to a resolution or checked against which pins support PWM, if you use a non-PWM pin you'll just get a lint warning, the value is still applied
- `analogReference(mode)` - stubbed as a no-op; `DEFAULT`, `INTERNAL`, and `EXTERNAL` are accepted without error
### Timing
- `delay(ms)` - pauses the sketch; scales with the speed slider; sleeps in 10ms chunks so the stop button responds quickly
- `delayMicroseconds(us)` - busy-wait with stop check
- `millis()` - returns sketch-perceived milliseconds since start; scales with the speed slider, not wall clock time
- `micros()` - same as `millis()` but in microseconds
### Math and Utilities
- `map(value, fromLow, fromHigh, toLow, toHigh)` - re-maps a number from one range to another; warns if `fromLow == fromHigh` (division by zero)
- `constrain(value, min, max)` - clamps a value to a range
- `abs(value)` - absolute value
- `min(a, b)` / `max(a, b)` - returns the smaller/larger of two values
- `random(max)` / `random(min, max)` - returns a random long; `randomSeed(seed)` is supported too, but isn't required since the simulation already starts fresh each run
### Tone
- `tone(pin, frequency)` - marks the buzzer component as active on the canvas; no actual audio is produced
- `tone(pin, frequency, duration)` - same, but auto-stops after `duration` ms
- `noTone(pin)` - stops the tone and marks the component inactive

### pulseIn
- `pulseIn(pin, value)` / `pulseIn(pin, value, timeout)` - has three paths depending on context:
  1. If the canvas has injected a pulse duration for that pin (distance sensor input field), returns immediately with that value
  2. If the pin belongs to a color sensor (TCS3200), reads from the color channel map
  3. Otherwise falls back to a polling loop: waits for idle → waits for pulse start → measures pulse end

### Serial
- `Serial.begin(baud)` - initializes serial output; baud rate is accepted but has no effect on timing in simulation
- `Serial.print(value)` - prints to the serial monitor without a newline; supports `int`, `float`, `String`, and `const char*`
- `Serial.println(value)` - same as `print` with a newline appended
- `Serial.available()` - returns the number of bytes waiting from the serial monitor input field
- `Serial.read()` - reads one byte from the serial input buffer
- `Serial1` / `Serial2` - same API as `Serial`; available on boards with multiple hardware UARTs (Mega 2560, Due, Teensy 4.1); each appears as a separate labeled pane in the serial monitor tab, but only `.begin()`/`.print()`/`.println()` are supported on them, `.available()`/`.read()` are not
- `Serial.printf(format, ...)` - printf-style formatting via `vsnprintf`; prints straight to the Serial Monitor like `print()`

### Wire / I2C
- `Wire.begin()` / `Wire.beginTransmission(address)` / `Wire.write(...)` / `Wire.endTransmission()` - queues bytes and always reports success; no bus errors are simulated
- `Wire.requestFrom(address, quantity)` / `Wire.available()` / `Wire.read()` - returns bytes from the Debug Panel's I2C tab for that address; unconfigured addresses just return zeros
- No slave mode, `Wire.onReceive`/`Wire.onRequest` don't exist

### SPI
- `SPI.begin()`, `SPISettings(...)`, `SPI.beginTransaction()`/`endTransaction()` - accepted but clock speed/bit order/data mode are ignored
- `SPI.transfer(byte)` / `SPI.transfer(buf, len)` - returns bytes from the Debug Panel's SPI tab, cycling through whatever sequence you configured there
- No per-device chip-select handling, drive your own CS pin with `digitalWrite()` same as real hardware

### AVR GPIO Registers
`DDRx`/`PORTx`/`PINx` work for ports B, C, and D (the Uno/Nano pin mapping) on **any** selected board, they don't change to match Mega/Due/Teensy's real port layout.
- `PORTB`/`DDRB`/`PINB` >> pins 8-13
- `PORTC`/`DDRC`/`PINC` >> pins 14-19 (A0-A5)
- `PORTD`/`DDRD`/`PIND` >> pins 0-7

### AVR Timer Registers
Timer1 (16-bit, `OCR1A`=pin 9, `OCR1B`=pin 10) and Timer2 (8-bit, `OCR2A`=pin 11, `OCR2B`=pin 3) registers are supported. **Timer0 doesn't exist** in VEMCODE, `millis()`/`delay()` are wall-clock based and unaffected by any timer register writes.

---
## Supported Libraries

**Overview:**
All supported libraries have to be custom libraries that follow the same functionality but manipulated to work within VEMCODE. The program DOES NOT support normal arduino or other embedded libraries becasue of how VEMCODE simulates the runtime.

### Servo
The Servo library is a relativly simple library that just contains the class for creating a servo with all included functions and variables:
- `int pin_` : stores the set pin -1 by default, if -1 its detached
- `int angle_` : stores the set angle, 0 by default
- `void attach(pin)` : Sets the set pin num
- `void write(angle)` : Set the angle and analog writes to the pin
- `int read()` : Returns stored angle 
- `bool attached()` : returns pin if it is greater or equal to 0
- `void detach()` : sets pin to be -1 to detach it

### SoftwareSerial
The Software Serial library is very similar to our current prints, but with new functions like read and peek to add serial communication functionality.
- `int rxPin, txPin;` : stores where the rx and tx pin are outputed
- `void begin(baud)` : sets the baud and begins the software serial
- `void print(x)` : prints x which can be any type
- `void println(x)` : prints x to a new line, x can be any type
- `int available()` : returns how many bytes are waiting, `0` if none
- `int read()` : pops and returns the next byte, `-1` if empty
- `int peek()` : same as `read()` but doesn't remove the byte, `-1` if empty
- `void write()` : converts the value to text and prints it, same as `print()`
- `bool listen()` : always returns `true`, multiple-instance RX contention isn't simulated
- `bool isListening()` : always returns `true`
- `bool overflow()` : always returns `false`

### EEPROM
The preprocessor just strips this in the code because the EEPROM library is baked into the runtime instead of it being seperated into a custom header file. Storage is 1024 bytes and does not persist between runs.
- `void EEPROM_write(int address, uint8_t value)` : writes a byte; silently does nothing if `address` is out of range
- `uint8_t EEPROM_read(int address)` : reads a byte; returns `0xFF` if `address` is out of range, matching real AVR behavior for unwritten cells
- `void EEPROM_update(int address, int value)` : same as `write`, but skips the write entirely if the value is already the same (saves simulated wear, same as real Arduino's `update()`)

### avr/wdt.h (Watchdog Timer)
The preprocessor just strips this in the code because the watchdog library is baked into the runtime instead of it being seperated into a custom header file.
- `void wdt_enable(int timeout_ms)` : starts the watchdog; takes a **plain millisecond value**, not real AVR's `WDTO_*` register constant, though those constants are still defined (as plain millisecond values) so existing sketches compile unchanged
- `void wdt_disable()` : stops the watchdog
- `void wdt_reset()` : resets the countdown, call this periodically or the watchdog will fire

### LiquidCrystal
The Liquid Crystal library is a helper for working with an LCD screen, and it's always modeled as a 16x2 display internally regardless of what size you tell it, larger displays (20x4, etc.) aren't actually simulated at that size.
- `LiquidCrystal(rs, en, d4, d5, d6, d7)` : only this one 6-pin constructor form exists, unlike real Arduino's several overloads (8-bit mode, with an RW pin)
- `void begin(cols, rows)` : `cols`/`rows` are accepted for compile compatibility but don't change the fixed 16x2 buffer
- `void clear()` : clears the display
- `void setCursor(col, row)` : clamped to 0-15/0-1
- `void write(char c)` : writes one character; non-printable bytes render as `*`
- `void print(x)` : prints `const char*`, `String`, `int`, `long`, `unsigned long`, or `float`
- `void createChar(uint8_t, uint8_t*)` : accepted but does nothing, custom glyphs aren't rendered

---
## Interrupts

### attachInterrupt
`attachInterrupt(pin, callback, mode)` accepts **any pin**, not just pins 2/3 like real Uno hardware. `digitalPinToInterrupt(pin)` is just an identity function, so passing a raw pin number works fine. Modes: `CHANGE`, `RISING`, `FALLING`.
- `detachInterrupt()` is not supported, calling it will fail to compile.

### ISR() Vector Macros
`ISR(VECTOR_NAME) { }` blocks are transformed by the preprocessor before compilation — the AVR macro wrapper is stripped, the body is renamed to `__vb_isr_VECTOR_NAME()`, and a `register_isr()` call is injected into `vb_setup()` automatically. `#include <avr/interrupt.h>` and `#include <avr/io.h>` are stripped silently.

Supported vectors and what triggers them:

| Vector | Trigger |
|---|---|
| `INT0_vect` | Pin 2 state change |
| `INT1_vect` | Pin 3 state change |
| `PCINT0_vect` | Any pin 8–13 state change |
| `PCINT1_vect` | Any pin 14–19 state change |
| `PCINT2_vect` | Any pin 0–7 state change |
| `USART_RX_vect` | User sends input via the serial monitor |
| `WDT_vect` | Watchdog timeout (see [avr/wdt.h](#avrwdth-watchdog-timer)) |
| `TIMER1_OVF_vect`, `TIMER1_COMPA_vect`, `TIMER1_COMPB_vect` | Timer1 overflow/compare-match (see [AVR Timer Registers](#avr-timer-registers)) |
| `TIMER2_OVF_vect`, `TIMER2_COMPA_vect`, `TIMER2_COMPB_vect` | Timer2 overflow/compare-match |

Any other vector name compiles but its handler will never fire, VEMCODE surfaces a warning: `"ISR vector 'X_vect' is not simulated — the handler will never fire"`. (`TIMER2_COMPA_vect`/`TIMER2_COMPB_vect` actually do fire despite sometimes showing this warning, safe to ignore for those two.)
### noInterrupts and interrupts
A single global flag gates all interrupt/ISR dispatch, `attachInterrupt` callbacks, `ISR()` vectors, and the watchdog/timer ISRs alike. There's no per-vector masking beyond your sketch's own `TIMSK`/`PCICR` register writes.

---
## AVR Compatibility

### PROGMEM and F() Macro
`PROGMEM` is an AVR-specific GCC attribute for storing data in flash. On x86 there is no flash distinction, so VEMCODE defines it as empty — sketches using `PROGMEM` compile without errors and data lands in normal RAM.
`F("string")` is defined as `(x)` — a no-op passthrough. Sketches using `F()` for flash string literals compile and run correctly without any changes.
### pgm_read_* Functions
`pgm_read_byte`, `pgm_read_word`, `pgm_read_dword`, and `pgm_read_float` are defined as plain pointer dereferences in the injected header. `#include <avr/pgmspace.h>` is stripped silently.
### AVR Assembly (asm / __asm__)
Inline assembly is transformed before compilation. Known instructions are mapped to their VEMCODE equivalents, everything else is stripped with a warning:
- `nop` — stripped silently
- `cli` → `api->noInterrupts()`
- `sei` → `api->interrupts()`
- Any unrecognized instruction — stripped with warning: `"Unrecognized assembly instruction 'X' removed"`

Both `asm` and `__asm__` are handled, with or without `__volatile__`/`volatile` and constraint strings.
### #ifdef ARDUINO
VEMCODE injects `#define ARDUINO 100` into the header, matching the value the real Arduino IDE defines. Sketches using `#ifdef ARDUINO` / `#ifndef ARDUINO` for cross-platform compatibility will take the correct branch.
### util/delay.h
`#include <util/delay.h>` is stripped silently. VEMCODE injects the following definitions in its place:
- `#define F_CPU 16000000UL`
- `_delay_ms(ms)` → `api->delay(...)`
- `_delay_us(us)` → `api->delayMicroseconds(...)`

---
## Multi-File Sketches
Any included custom .h and c++ files will be included in the compiler pass as long as they are in the same directory as the loaded/main sketch file.

---
## Debugging Tools

### Serial Monitor
The serial montor works as expected as a serial output for the sketch (`Serial.print()`, `Serial.println()`, etc). The serial monitor panel adapts to the selected board and checks how many supported serial monitors are available, Arudino Uno has 1, Teensy 4.1 has 3, etc.

### Variable Watch
The variable watch pannel shows a table with three columns: Variable, Type, and Value. Add a variable to track first (an "+ Add variable" row, name + type), then call this in your sketch to keep its value updating:
```c++
watch_variable("LABEL", value);
```

### Signal Timeline
The signal timeline shows a simple logic analyzer style graph showing the changing values of HIGH and LOW over time. Pins aren't added automatically, type the pin number into the field and click "+ Add pin" to start tracking it.

---
## Differences from Real Hardware

### Timing
VEMCODE has trouble simulating code that runs at sub millisecond level timing and needs to be consistent. This is unnavoidable because we are simulating on an x86 system with inconsistent timing at that level.

### Speed Slider and Timing Functions
The Speed slider is a way to adjust the simulation speed of your sketch by modifying every timing funciton that simulates a sleep on the processor like a `delay()`. Since it is just a multiplication on how fast the delay and other timing functions are processed, it could cause inconsistencies at faster speeds.

### Floating INPUT Pins
If a pin is read that is not attached to a component, VEMCODE simulates the floating pin by returning a random value to approximate what a floating pin does.

### Button Bounce
VEMCODE emulates button bounce when a default button is called. This can be disabled by changing the detected button to be a clean or ideal button. This can be done by changing the funciton call to add IDEAL or CLEAN to it.

### EEPROM Persistence
EEPROM does not persist between runs, every time you stop and re-run a sketch it starts back at whatever it was last written to during that same run, not saved to disk anywhere.

### ISR Timing
Hardware timer interrupts (`TIMER1_*`/`TIMER2_*` vectors) are checked by a background thread polling roughly every 1ms of real time, not a true cycle-accurate hardware timer. Very high-frequency timer interrupts won't fire with the same precision they would on real hardware.

---
## What VEMCODE Does Not Support
- Arbitrary third-party Arduino libraries, only the ones listed above.
- `detachInterrupt()`.
- I2C/SPI slave mode (`Wire.onReceive`/`onRequest`), or SPI clock speed/bit order/data mode.
- Timer0, or any AVR register outside ports B/C/D and Timer1/Timer2.
- Board-accurate AVR register layouts on Mega/Due/Teensy, `DDRx`/`PORTx`/`PINx` always use the Uno pin mapping regardless of selected board.
- EEPROM persistence across runs.
- LCD sizes other than 16x2, and custom LCD characters (`createChar()` is a no-op).
- `Servo.writeMicroseconds()`.
- Real audio output for `tone()`, it's visual-only on the canvas.

---
## Common Errors and What They Mean
These are lint warnings VEMCODE prints to the Serial Monitor before running your sketch, or errors from the compiler itself:
- **"Pin N is not available on the \<board\>"**: the pin number you used doesn't exist on the currently selected board.
- **"Pin N does not support PWM on the \<board\> — analogWrite() will have no effect"**: a heads-up only, the simulator still applies the write, it just wouldn't do anything on real hardware.
- **"attachInterrupt(0, ...) uses an interrupt number, not a pin number"**: you passed a raw interrupt number instead of a pin, use `digitalPinToInterrupt(pin)`.
- **"delay() inside '...' will hang on real Arduino"**: you called `delay()` from what looks like an interrupt handler, interrupts are disabled during real ISR execution, so this would hang on actual hardware even though it runs fine in simulation.
- **"Pin '...' is defined as an expression"**: you wrote something like `#define LED_PIN 1+2`, the detector can't evaluate that, so the component won't appear on the canvas even though the code compiles.
- **"Pin N is used with digitalWrite() but pinMode() was never called"**: on real hardware this pin would default to `INPUT`, double check you meant to configure it as `OUTPUT`.
- **"'x' is shared with an ISR but not declared volatile"**: this may happen to work in simulation but is likely to break on real hardware, add `volatile` to the variable.
- **"ISR vector 'X_vect' is not simulated"**: see the [ISR() Vector Macros](#isr-vector-macros) table for what's actually supported.
- **"'X' was not declared in this scope"**: a real compile error, usually a typo or a missing `#define`/variable.

---
## Tips and Best Practices
- Name your pins descriptively (`LED_PIN`, `TRIG_PIN`, etc.), the circuit detector only works off names, hardcoded numbers won't show up on the canvas.
- If timing precision matters for a button, use a clean button (`CLEAN`/`IDEAL` in the name) to skip the ~10ms bounce simulation.
- Use `watch_variable("LABEL", value);` liberally while debugging, it's cheap and shows up live in the Variable Watch tab.
- Check the Serial Monitor for warnings even when the sketch compiles fine, several of the checks above only ever show up there, not as editor highlights.
- If a component isn't showing up on the canvas, check that its pin isn't hardcoded or defined as an expression (see [Hardcoded Pin Numbers](#hardcoded-pin-numbers) and [Pins Defined as Expressions](#pins-defined-as-expressions)).
- Right-click a function in the editor (`digitalWrite`, `Serial.print`, etc.) for a quick signature/params/return-value popup instead of switching over to [API_REFERENCE.md](API_REFERENCE.md).
