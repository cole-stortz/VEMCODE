# VEMCODE Sketch Writing Guide

This guide covers everything you need to know to write Arduino sketches that run correctly in VEMCODE. Most standard Arduino code works without modification — this document covers what's supported, what isn't, and VEMCODE-specific features.

---

## Structure

A VEMCODE sketch is a standard Arduino sketch. Write it exactly as you would for a real board:

```cpp
void setup() {
    pinMode(13, OUTPUT);
}

void loop() {
    digitalWrite(13, HIGH);
    delay(500);
    digitalWrite(13, LOW);
    delay(500);
}
```

VEMCODE's preprocessor transforms this into a shared library format automatically when you hit Run. You never write `vb_init`, `vb_setup`, or `vb_loop` by hand.

---

## Targeting a Specific Board

Place a `// @board` comment near the top of your sketch to tell VEMCODE which board to simulate:

```cpp
// @board Teensy 4.1
// @board Arduino Mega 2560
// @board Arduino Uno
```

The name must match exactly (including capitalization). If the name is unrecognized, VEMCODE silently falls back to whatever board is selected in Settings. The matched board is saved to `settings.ini` and persists across restarts.

Without a `// @board` comment, VEMCODE uses the board selected in the Settings dialog.

---

## Supported API

### GPIO

```cpp
pinMode(pin, mode);         // mode: INPUT, OUTPUT, INPUT_PULLUP
digitalWrite(pin, value);   // value: HIGH or LOW
int val = digitalRead(pin); // returns HIGH or LOW
```

### Analog

```cpp
analogWrite(pin, value);    // value: 0–255 (Uno/Nano/Mega), 0–4095 (Due/Teensy)
int val = analogRead(pin);  // returns 0–1023; pin can be A0–A5 or 14–19
```

### Timing

```cpp
delay(ms);                  // milliseconds — interruptible by Stop button
delayMicroseconds(us);      // microseconds — busy-wait, not real-time accurate
unsigned long t = millis(); // ms since sketch started
unsigned long t = micros(); // µs since sketch started
```

`delay` sleeps in 10ms chunks and checks for the Stop signal between chunks. Very long delays (seconds) will still respond to Stop promptly. `delayMicroseconds` busy-waits in 50µs chunks — it is not real-time accurate but is fine for short pulses like the HC-SR04 trigger sequence.

### Serial

```cpp
Serial.begin(9600);         // any baud rate — value is displayed but not enforced
Serial.print(value);        // int, float, char, String, const char*
Serial.println(value);      // same, adds newline
Serial.available();         // returns number of bytes in the receive buffer
Serial.read();              // returns next byte (-1 if empty)
```

Serial output appears in the Serial Monitor panel. Input is sent by typing in the input box and clicking Send (or pressing Enter), which injects the text followed by `\n`.

#### Print with format base

```cpp
Serial.print(value, HEX);   // prints in hexadecimal
Serial.print(value, BIN);   // prints in binary
Serial.print(value, OCT);   // prints in octal
Serial.print(value, DEC);   // prints in decimal (default)
```

#### Print float with precision

```cpp
Serial.print(3.14159f, 2);  // prints "3.14"
Serial.println(voltage, 3); // prints "3.142\n"
```

### PulseIn

```cpp
unsigned long duration = pulseIn(pin, HIGH, 1000000UL);
```

All three arguments are required — there is no default timeout in VEMCODE's function pointer table. The third argument is the timeout in microseconds.

**Three execution paths depending on context:**
- If the canvas Distance Sensor input has a value set, `pulseIn` returns immediately with the pre-computed duration (fast path).
- If the pin is a registered TCS3200 OUT pin, it returns the color channel period based on the current S2/S3 pin states.
- Otherwise, it polls the pin state in a loop (slow path — works for simple simulated toggles).

### watch_variable

VEMCODE-specific. Sends a named integer value to the Variable Watch panel in real time:

```cpp
watch_variable("speed", motorSpeed);
watch_variable("distance_cm", distance);
```

Values appear as a live table in the Variable Watch panel. Only `int` values are supported — cast floats or other types before passing.

---

## Constants

These are all available without any `#include`:

```cpp
// Pin modes
INPUT         // 0
OUTPUT        // 1
INPUT_PULLUP  // 2

// Digital values
LOW   // 0
HIGH  // 1

// Analog pins (map to internal pin numbers 14–19)
A0    // 14
A1    // 15
A2    // 16
A3    // 17
A4    // 18
A5    // 19

// Print bases
HEX   // 16
DEC   // 10
OCT   // 8
BIN   // 2
```

---

## String Class

The `String` class is built in — no `#include` needed:

```cpp
String s = "hello";
String s2 = String(42);         // int → String
s += " world";
s += 99;                        // int append
int len = s.length();
bool empty = s.isEmpty();
String upper = s.toUpperCase();
String lower = s.toLowerCase();
String trimmed = s.trim();
String sub = s.substring(2);    // from index 2 to end
String sub = s.substring(2, 5); // from 2, length = 3
int idx = s.indexOf("lo");      // -1 if not found
bool yes = s.startsWith("he");
bool yes = s.endsWith("ld");
bool yes = s.contains("ell");
char c = s.charAt(0);
s.setCharAt(0, 'H');
char c = s[0];                  // same as charAt
int n = s.toInt();
float f = s.toFloat();
s == "hello"                    // comparison works
s != "world"
s.equals("hello")
```

**String literal gotcha:** `"literal" + String(x)` does not compile because the left operand is a raw `const char*`, not a `String`. Always start with a `String`:

```cpp
// Wrong:
String msg = "value: " + String(x);

// Right:
String msg = String("value: ") + x;
// or:
String msg = "value: ";
msg += x;
```

---

## Math Functions

All built in, no `#include` needed:

```cpp
map(value, in_min, in_max, out_min, out_max)  // linear scale
constrain(value, low, high)                    // clamp
abs(x)                                         // absolute value
min(a, b)
max(a, b)
random(max)                                    // 0 to max-1
random(min, max)                               // min to max-1
randomSeed(seed)                               // seeds random()
```

Standard C++ headers `<cmath>`, `<cstdlib>`, `<cstdint>`, `<climits>` are also available, so `sqrt`, `pow`, `sin`, `cos`, `floor`, `ceil`, `INT_MAX`, `uint8_t`, etc. all work.

---

## Servo

Include and use exactly as you would with a real Arduino:

```cpp
#include <Servo.h>

Servo myServo;

void setup() {
    myServo.attach(9);
}

void loop() {
    myServo.write(90);
    delay(1000);
    myServo.write(0);
    delay(1000);
}
```

The `#include <Servo.h>` line is stripped by the preprocessor and replaced with a built-in `Servo` class. The canvas shows the servo angle in real time based on `analogWrite` values.

---

## LiquidCrystal (16×2 LCD)

Include and use exactly as you would with a real Arduino:

```cpp
// @board Arduino Uno
#include <LiquidCrystal.h>

#define LCD_RS 12
#define LCD_EN 11
#define LCD_D4  5
#define LCD_D5  4
#define LCD_D6  3
#define LCD_D7  2

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

void setup() {
    lcd.begin(16, 2);
    lcd.print("Hello, VEMCODE!");
}

void loop() {
    lcd.setCursor(0, 1);
    lcd.print(millis() / 1000);
    delay(500);
}
```

The `#include <LiquidCrystal.h>` line is stripped by the preprocessor and replaced with a built-in `LiquidCrystal` class. Text passed to `lcd.print()` appears on the canvas LCD in real time.

**Supported methods:**

```cpp
lcd.begin(cols, rows);        // initializes display — call in setup()
lcd.print("text");            // const char*, String, int, float
lcd.setCursor(col, row);      // row 0 or 1; col is ignored (text always starts at col 0)
lcd.clear();                  // clears both rows
```

Text is truncated to 16 characters per row. The canvas LCD shows two rows of monospace text, updated immediately whenever `lcd.print()` is called.

**Canvas detection:** VEMCODE detects the LCD from the `LCD_RS`, `LCD_EN`, `LCD_D4`–`LCD_D7` `#define` names. Use those naming conventions and the LCD component will appear on the canvas automatically.

---

## PROGMEM and F()

Flash-string helpers are no-ops in VEMCODE — they compile without modification but do nothing special:

```cpp
const char msg[] PROGMEM = "hello";  // PROGMEM is defined as nothing
Serial.println(F("hello"));          // F(x) expands to x
```

---

## Component Detection

VEMCODE infers what components are in your circuit by reading your sketch source. To get accurate canvas rendering, follow these naming conventions for your `#define` pin names:

| Component | Keyword examples |
|---|---|
| LED | `LED_PIN`, `LIGHT_PIN`, `LAMP_PIN`, `INDICATOR_PIN` |
| Button | `BTN_PIN`, `BUTTON_PIN`, `KEY_PIN` |
| Switch | `SWITCH_PIN`, `SW_PIN`, `TOGGLE_PIN` |
| Buzzer | `BUZZER_PIN`, `BUZZ_PIN`, `SPEAKER_PIN`, `TONE_PIN`, `PIEZO_PIN` |
| Servo | `SERVO_PIN`, `SRV_PIN` |
| Potentiometer | `POT_PIN`, `POTENTIOMETER_PIN`, `DIAL_PIN` |
| Light Sensor | `PHOTO_PIN`, `LDR_PIN`, `PHOTORESISTOR_PIN` |
| Temp Sensor | `TEMP_PIN`, `TEMPERATURE_PIN`, `THERMISTOR_PIN` |
| Analog Sensor | `SENSOR_PIN`, `ANALOG_PIN`, `ADC_PIN` |
| Distance Sensor (HC-SR04) | paired `TRIG_PIN` + `ECHO_PIN` defines |
| H-Bridge Motor | grouped `MOTOR1_PWM`, `MOTOR1_CW`, `MOTOR1_ANTI` defines |
| Color Sensor (TCS3200) | `s0Pins[]`, `s1Pins[]`, `s2Pins[]`, `s3Pins[]`, `sensorOut[]` arrays |
| LCD (16×2) | `LCD_RS`, `LCD_EN`, `LCD_D4`–`LCD_D7` defines (6-pin group) |

Components detected via `analogRead(A0)` calls (with no matching `#define`) fall back to `AnalogSensor`.

---

## What Doesn't Work

These are permanent limitations of the compile-to-native-DLL architecture:

| Feature | Why it doesn't work |
|---|---|
| `asm volatile(...)` | AVR assembly instructions don't exist on x86 |
| `ISR(TIMER1_OVF_vect)` etc. | Hardware interrupt vectors don't exist in simulation |
| Real electrical behavior | No voltage/current/short-circuit simulation |
| `int` wrapping at 32767 | `int` is 32-bit on x86 vs 16-bit on AVR — use `int16_t` if exact-width behavior matters |
| No audio output | `tone()`/`noTone()` track frequency state and update the buzzer canvas colour, but produce no actual sound |

If your sketch uses `int16_t`, `uint8_t`, or other fixed-width types from `<cstdint>`, they work correctly.

---

## Tips

- **Speed slider** — use the toolbar speed slider to slow down or speed up `delay` calls. Useful for slowing down fast blink sketches or speeding through long waits.
- **Hot reload** — VEMCODE polls the compiled `.so`/`.dll` for changes. If you edit and re-run, the sketch reloads automatically without restarting the app.
- **Safety delay** — if your sketch has no `delay` call at all, VEMCODE injects a `delay(10)` before the closing `}` of `loop()` to prevent the simulation thread from spinning at 100% CPU.
- **Serial input** — `Serial.available()` and `Serial.read()` work. Type in the serial monitor input box and click Send to inject bytes into the sketch's receive buffer.