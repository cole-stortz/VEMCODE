#pragma once

#include <QString>

struct BoardProfile {
    const char* name;
    const char* chip;
    int pin_count;
    int analog_offset;
    int analog_count;
    int pwm_resolution;
    int serial_count;  // number of hardware serial ports (capped at runtime support: Serial + Serial1 + Serial2)
};

static const BoardProfile BOARD_UNO    = {"Arduino Uno",       "ATmega328P",   20, 14,  6,  255, 1};
static const BoardProfile BOARD_NANO   = {"Arduino Nano",      "ATmega328P",   22, 14,  8,  255, 1};
static const BoardProfile BOARD_MEGA   = {"Arduino Mega 2560", "ATmega2560",   70, 54, 16,  255, 3};
static const BoardProfile BOARD_DUE    = {"Arduino Due",       "AT91SAM3X8E",  66, 54, 12, 4095, 3};
static const BoardProfile BOARD_TEENSY = {"Teensy 4.1",        "IMXRT1062",    42, 14, 18, 4095, 3};

// Looks up the profile matching a board's display name (e.g. "Arduino Mega 2560").
// Returns false and leaves `out` untouched if `name` isn't a recognized board.
inline bool boardProfileForName(const QString& name, BoardProfile& out) {
    if (name == "Arduino Nano")       { out = BOARD_NANO;   return true; }
    if (name == "Arduino Mega 2560")  { out = BOARD_MEGA;   return true; }
    if (name == "Arduino Due")        { out = BOARD_DUE;    return true; }
    if (name == "Teensy 4.1")         { out = BOARD_TEENSY; return true; }
    if (name == "Arduino Uno")        { out = BOARD_UNO;    return true; }
    return false;
}

// Same lookup, but returns `fallback` instead of reporting failure.
inline BoardProfile boardProfileForName(const QString& name, const BoardProfile& fallback = BOARD_UNO) {
    BoardProfile out;
    return boardProfileForName(name, out) ? out : fallback;
}