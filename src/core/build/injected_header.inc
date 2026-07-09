#include "src/core/runtime/arduinoapi.h"
#include <cstdarg>
#include <cstdio>
#include <string>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <climits>
using namespace vb;

typedef uint8_t  byte;
typedef uint16_t word;

#define ARDUINO 100

static ArduinoAPI* api = nullptr;

extern "C" VB_EXPORT
void vb_init(ArduinoAPI* a) { api = a; }

template<typename T>
inline void Serial_println(T v) { api->Serial_println(std::to_string(v).c_str()); }
inline void Serial_println(const char* s) { api->Serial_println(s); }
inline void Serial_println(char* s)       { api->Serial_println(s); }
inline void Serial_println(char c) { char s[2] = {c, '\0'}; api->Serial_println(s); }
template<typename T>
inline void Serial_print(T v) { api->Serial_print(std::to_string(v).c_str()); }
inline void Serial_print(const char* s) { api->Serial_print(s); }
inline void Serial_print(char* s)       { api->Serial_print(s); }
inline void Serial_print(char c) { char s[2] = {c, '\0'}; api->Serial_print(s); }

const int HEX = 16;
const int DEC = 10;
const int OCT = 8;
const int BIN = 2;
template<typename T>
inline void Serial_print(T v, int base) {
    std::stringstream ss;
    if (base == HEX) ss << std::hex << v;
    else if (base == OCT) ss << std::oct << v;
    else if (base == BIN) {
        std::string bits;
        for (int i = sizeof(T) * 8 - 1; i >= 0; --i) bits += (char)('0' + ((v >> i) & 1));
        size_t first = bits.find('1');
        ss << (first == std::string::npos ? "0" : bits.substr(first));
    } else ss << v;
    api->Serial_print(ss.str().c_str());
}
template<typename T>
inline void Serial_println(T v, int base) {
    std::stringstream ss;
    if (base == HEX) ss << std::hex << v;
    else if (base == OCT) ss << std::oct << v;
    else if (base == BIN) {
        std::string bits;
        for (int i = sizeof(T) * 8 - 1; i >= 0; --i) bits += (char)('0' + ((v >> i) & 1));
        size_t first = bits.find('1');
        ss << (first == std::string::npos ? "0" : bits.substr(first));
    } else ss << v;
    api->Serial_println(ss.str().c_str());
}

inline void Serial_print(float v, int decimals) {
    std::stringstream ss;
    ss.precision(decimals);
    ss << std::fixed << v;
    api->Serial_print(ss.str().c_str());
}
inline void Serial_println(float v, int decimals) {
    std::stringstream ss;
    ss.precision(decimals);
    ss << std::fixed << v;
    api->Serial_println(ss.str().c_str());
}

inline void Serial_printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    api->Serial_print(buf);
}

// Arduino String class -- wraps std::string for sketch compatibility
class String {
public:
    String() {}
    String(const char* s) : s_(s ? s : "") {}
    String(int v)         : s_(std::to_string(v)) {}
    String(long v)        : s_(std::to_string(v)) {}
    String(float v)       : s_(std::to_string(v)) {}
    String(double v)      : s_(std::to_string(v)) {}
    String(std::string s) : s_(s) {}

    // Concatenation
    String operator+(const String& o) const { return String(s_ + o.s_); }
    String operator+(const char* o)   const { return String(s_ + o); }
    String operator+(char c)          const { return String(s_ + c); }
    String& operator+=(const String& o) { s_ += o.s_; return *this; }
    String& operator+=(const char* o)   { s_ += o;    return *this; }
    String& operator+=(char c)          { s_ += c;    return *this; }
    String& operator+=(int v)           { s_ += std::to_string(v); return *this; }

    // Comparison
    bool operator==(const String& o) const { return s_ == o.s_; }
    bool operator==(const char* o)   const { return s_ == o; }
    bool operator!=(const String& o) const { return s_ != o.s_; }
    bool operator!=(const char* o)   const { return s_ != o; }
    bool equals(const String& o)     const { return s_ == o.s_; }

    // Info
    int    length()    const { return (int)s_.size(); }
    bool   isEmpty()   const { return s_.empty(); }
    const char* c_str() const { return s_.c_str(); }

    // Conversion
    int    toInt()     const { return s_.empty() ? 0 : std::stoi(s_); }
    float  toFloat()   const { return s_.empty() ? 0.0f : std::stof(s_); }

    // Search
    int indexOf(const String& s, int from = 0) const {
        auto p = s_.find(s.s_, from);
        return p == std::string::npos ? -1 : (int)p;
    }
    int indexOf(char c, int from = 0) const {
        auto p = s_.find(c, from);
        return p == std::string::npos ? -1 : (int)p;
    }
    bool startsWith(const String& s) const { return s_.rfind(s.s_, 0) == 0; }
    bool endsWith(const String& s)   const {
        return s_.size() >= s.s_.size() &&
               s_.compare(s_.size() - s.s_.size(), s.s_.size(), s.s_) == 0;
    }
    bool contains(const String& s)   const { return s_.find(s.s_) != std::string::npos; }

    // Manipulation
    String substring(int from, int to = -1) const {
        if (to < 0) return String(s_.substr(from));
        return String(s_.substr(from, to - from));
    }
    String replace(const String& from, const String& to) const {
        std::string r = s_;
        size_t pos = 0;
        while ((pos = r.find(from.s_, pos)) != std::string::npos) {
            r.replace(pos, from.s_.size(), to.s_);
            pos += to.s_.size();
        }
        return String(r);
    }
    String toLowerCase() const {
        std::string r = s_;
        for (auto& c : r) c = tolower(c);
        return String(r);
    }
    String toUpperCase() const {
        std::string r = s_;
        for (auto& c : r) c = toupper(c);
        return String(r);
    }
    String trim() const {
        size_t start = s_.find_first_not_of(" \t\n\r");
        size_t end   = s_.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return String("");
        return String(s_.substr(start, end - start + 1));
    }
    char charAt(int i) const { return (i >= 0 && i < (int)s_.size()) ? s_[i] : 0; }
    void setCharAt(int i, char c) { if (i >= 0 && i < (int)s_.size()) s_[i] = c; }
    char operator[](int i) const  { return charAt(i); }

    // Stream support -- allows Serial.println(String)
    operator const char*() const { return s_.c_str(); }

private:
    std::string s_;
};

// Serial overloads for String type -- must come after String class definition
inline void Serial_println(const String& s) { api->Serial_println(s.c_str()); }
inline void Serial_print(const String& s)   { api->Serial_print(s.c_str()); }

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
inline long constrain(long x, long lo, long hi) {
    return x < lo ? lo : x > hi ? hi : x;
}
inline long   vb_abs(long x)         { return x < 0 ? -x : x; }
inline long   vb_min(long a, long b) { return a < b ? a : b; }
inline long   vb_max(long a, long b) { return a > b ? a : b; }
inline long   random(long max)       { return rand() % max; }
inline long   random(long min, long max) { return min + rand() % (max - min); }
#define abs(x) vb_abs(x)
#define min(a,b) vb_min(a,b)
#define max(a,b) vb_max(a,b)

// pulseIn with optional timeout matching real Arduino behavior
inline unsigned long pulseIn(int pin, int value, unsigned long timeout = 1000000UL) {
    return api->pulseIn(pin, value, timeout);
}

// PROGMEM / F() -- no-ops so common Arduino sketches compile without modification
#define PROGMEM
#define F(x) (x)

// AVR pgmspace.h compatibility -- data is already in RAM on x86, so these are plain pointer reads
#define pgm_read_byte(addr)  (*(const uint8_t *)(addr))
#define pgm_read_word(addr)  (*(const uint16_t*)(addr))
#define pgm_read_dword(addr) (*(const uint32_t*)(addr))
#define pgm_read_float(addr) (*(const float   *)(addr))

// util/delay.h compatibility -- map to the runtime delay functions
#define F_CPU 16000000UL
#define _delay_ms(ms) api->delay((unsigned long)(ms))
#define _delay_us(us) api->delayMicroseconds((unsigned long)(us))

// Arduino analog reference constants (Uno values; simulation ignores them)
#define DEFAULT  0
#define EXTERNAL 1
#define INTERNAL 3

// analogReference -- simulation ignores ADC reference voltage; stub so sketches compile
inline void analogReference(int) {}

// randomSeed -- seeds the same rand() used by random()
inline void randomSeed(unsigned long seed) { srand((unsigned int)seed); }

inline void tone(int pin, int frequency)                  { api->tone(pin, frequency, 0); }
inline void tone(int pin, int frequency, int duration_ms) { api->tone(pin, frequency, duration_ms); }
inline void noTone(int pin)                               { api->noTone(pin); }

inline int digitalPinToInterrupt(int pin) { return pin; }

// Sleep mode constants (matching AVR values)
#define SLEEP_MODE_IDLE        0
#define SLEEP_MODE_ADC         1
#define SLEEP_MODE_PWR_DOWN    2
#define SLEEP_MODE_PWR_SAVE    3
#define SLEEP_MODE_STANDBY     6
#define SLEEP_MODE_EXT_STANDBY 7

// sleep_mode() convenience: enable → cpu → disable (mirrors avr/sleep.h macro)
inline void sleep_mode() {
    api->sleep_enable();
    api->sleep_cpu();
    api->sleep_disable();
}

// Watchdog definitions
#define WDTO_15MS   15
#define WDTO_30MS   30
#define WDTO_60MS   60
#define WDTO_120MS  120
#define WDTO_250MS  250
#define WDTO_500MS  500
#define WDTO_1S    1000
#define WDTO_2S    2000
#define WDTO_4S    4000
#define WDTO_8S    8000

