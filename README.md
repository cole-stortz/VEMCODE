# VirtualBench

An open-source embedded systems development tool for writing, simulating, testing, and debugging Arduino code — no hardware required.

![Status](https://img.shields.io/badge/status-early%20development-orange)
![Platform](https://img.shields.io/badge/platform-Windows-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## What it does

Write standard Arduino sketches directly in the built-in editor and simulate them instantly — no Arduino board, no USB cable, no waiting.

- **Write** Arduino code in a syntax-highlighted editor with line numbers and error highlighting
- **Simulate** instantly — hit Run and your sketch compiles and executes in milliseconds
- **Visualize** — the circuit canvas auto-detects components from your code (`#define LED_PIN 13` → LED appears on the canvas)
- **Debug** — serial monitor shows `Serial.print` output live, signal timeline shows pin waveforms like a logic analyzer
- **Interact** — click button components on the canvas to inject pin state into the running simulation
- **Hot-reload** — edit your sketch, hit Run again, simulation restarts instantly

## Screenshots

*Coming soon*

## Getting started

### Prerequisites

- Windows 10/11
- Qt 6.x with MinGW 64-bit — [download from qt.io](https://www.qt.io/download)
- CMake 3.20+
- Ninja build system (included with Qt)

### Build from source

```bash
# Clone the repo
git clone https://github.com/cole-stortz/VirtualBench.git
cd VirtualBench

# Configure
cmake -B build -S . -G "Ninja" \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" \
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" \
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe"

# Build
cmake --build build

# Deploy Qt runtime (first time only)
C:\Qt\6.11.1\mingw_64\bin\windeployqt.exe app\VirtualBench.exe
```

### Run

```bash
.\app\VirtualBench.exe
```

## Writing sketches

VirtualBench accepts standard Arduino syntax — write exactly what you would write for a real Arduino:

```cpp
#define LED_PIN    13
#define BUTTON_PIN  2

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Button pressed");
    } else {
        digitalWrite(LED_PIN, LOW);
    }
    delay(50);
}
```

The preprocessor automatically transforms your sketch into the VirtualBench runtime format before compilation. You never need to write any boilerplate.

## Circuit auto-detection

VirtualBench reads your `#define` statements and `pinMode` calls to automatically populate the circuit canvas:

| Pattern | Component |
|---|---|
| `#define LED_PIN` + `pinMode(OUTPUT)` | LED |
| `#define BUTTON_PIN` + `pinMode(INPUT_PULLUP)` | Button (clickable) |
| `#define BUZZER_PIN` + `pinMode(OUTPUT)` | Buzzer |
| `#define SERVO_PIN` | Servo |
| `Serial.begin(...)` | Serial monitor |

## Project structure

```
VirtualBench/
├── app/                        # Runtime — exe + Qt DLLs
│   └── sketches/               # Your saved sketches live here
├── src/
│   ├── main.cpp
│   ├── ui/                     # Qt6 UI
│   │   ├── mainwindow.cpp/h
│   │   ├── canvaswidget.cpp/h
│   │   ├── signaltimeline.cpp/h
│   │   ├── codehighlighter.cpp/h
│   │   └── linenumberarea.h
│   └── core/
│       ├── runtime/            # Virtual Arduino runtime
│       │   ├── arduinoapi.h
│       │   └── arduinoruntime.cpp/h
│       ├── host/               # Sketch DLL loader + hot-reload
│       │   ├── sketchhost.cpp/h
│       │   └── sketchhostthread.cpp/h
│       ├── build/              # Compiler pipeline + preprocessor
│       │   ├── compiler.cpp/h
│       │   └── preprocessor.cpp/h
│       └── circuit/            # Auto circuit detection
│           └── circuitdetector.cpp/h
├── sketches/                   # Example sketches
└── CMakeLists.txt
```

## Architecture

VirtualBench compiles your sketch into a `.dll` using the system C++ compiler and loads it at runtime via `LoadLibrary`. The sketch DLL calls back into the host through a function pointer table (`ArduinoAPI`) — so `digitalWrite(13, HIGH)` in your sketch calls `impl_digitalWrite` in the host, which updates the canvas and signal timeline in real time.

Hot-reload works by watching the DLL file for changes and calling `FreeLibrary` + `LoadLibrary` while the simulation is running — no restart needed.

## Roadmap

- [ ] Variable watch panel
- [ ] Potentiometer / analog input simulation
- [ ] More component types (servo animation, buzzer tone)
- [ ] macOS / Linux support
- [ ] Installer / packaged release
- [ ] Additional board support (Nano, Mega)

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Pull requests welcome. This project is early — if you find a bug or want a feature, open an issue.