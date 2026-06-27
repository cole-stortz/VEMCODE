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
- `Servo NAME`                     >> detects a servo as NAME
- `LiquidCrystal lcd(8, 9, 10, 11, 12, 13)` >> detects a LCD at specified pins
### Naming Conventions That Work
VEMCODE matches keywords against your pin names (case-insensitive). Any pin name containing one of these words will be detected as that component:

| Component             | Keywords                                                    |
| --------------------- | ----------------------------------------------------------- |
| LED                   | `LED`, `LIGHT`, `LAMP`, `INDICATOR`                         |
| Button (bouncy)       | `BTN`, `BUTTON`, `KEY`                                      |
| Button (clean)        | `TACT`, `CLEAN`, `IDEAL`                                    |
| Switch                | `SWITCH`, `SW`, `TOGGLE`                                    |
| Buzzer                | `BUZZER`, `BUZZ`, `SPEAKER`, `TONE`, `PIEZO`                |
| Servo                 | `SERVO`, `SRV`                                              |
| H-Bridge Motor        | `MOTOR` (multi-pin group: `PWM`, `DIR`/`CW`, `ANTI`)        |
| Potentiometer         | `POT`, `POTENTIOMETER`, `DIAL`                              |
| Light Sensor          | `PHOTO`, `LDR`, `LIGHT_SENSOR`, `PHOTORESISTOR`             |
| Temperature Sensor    | `TEMP`, `TEMPERATURE`, `THERMISTOR`                         |
| Generic Analog Sensor | `SENSOR`, `ANALOG`, `ADC`                                   |
| LCD                   | `LCD`, `DISPLAY`, `SCREEN` (or `LiquidCrystal` constructor) |
| Distance Sensor       | Paired `TRIG` + `ECHO` defines                              |
| Color Sensor          | Paired `S0`, `S1`, `S2`, `S3`, `OUT` defines (TCS3200)      |

Clean buttons (`TACT`/`CLEAN`/`IDEAL`) are checked before `BTN`/`BUTTON` so they take priority if both keywords appear in the name.
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
- `analogWrite(pin, value)` - writes a PWM value (0–255 on AVR boards, 0–4095 on Due/Teensy); also logs to the signal timeline
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
- `random(max)` / `random(min, max)` - returns a random long; no `randomSeed()` needed since the simulation always starts fresh
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
- `Serial1` / `Serial2` - same API as `Serial`; available on boards with multiple hardware UARTs (Mega 2560, Due, Teensy 4.1); each appears as a separate labeled pane in the serial monitor tab

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
- `int rxPin, txPin;` : stores where the rx and tx pin are outputed
- `void begin(baud)` : sets the baud and begins the software serial
- `void print(x)` : prints x which can be any type
- `void println(x)` : prints x to a new line, x can be any type
- `int available()` : 
- `int read()` :
- `int peek()` :
- `void write()` : 
- `bool listen()` : 
- `bool isListening()` : 
- `bool overflow()` : 

### EEPROM
baked into the runtime

### avr/wdt.h (Watchdog Timer)
baked into the runtime

### liquidcrystal

---
## Interrupts

### attachInterrupt

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

Unsupported vectors (`TIMER1_OVF_vect`, `TIMER2_OVF_vect`, `TIMER1_COMPA_vect`, `WDT_vect`, etc.) are accepted without a compile error but will never fire — VEMCODE surfaces a warning: `"ISR vector 'X_vect' is not simulated — the handler will never fire"`.
### noInterrupts and interrupts

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

---
## Debugging Tools

### Serial Monitor

### Variable Watch

### Signal Timeline

---
## Differences from Real Hardware

### Timing

### Speed Slider and Timing Functions

### Floating INPUT Pins

### Button Bounce

### EEPROM Persistence

### ISR Timing

---
## What VEMCODE Does Not Support

---
## Common Errors and What They Mean

---
## Tips and Best Practices
