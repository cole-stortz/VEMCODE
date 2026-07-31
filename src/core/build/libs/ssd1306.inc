#ifndef VB_SSD1306_H
#define VB_SSD1306_H

// Modern Adafruit_SSD1306 examples use these prefixed names specifically to
// avoid colliding with sketch-defined WHITE/BLACK, so that's all this offers
// -- no bare legacy aliases.
constexpr uint16_t SSD1306_BLACK   = 0;
constexpr uint16_t SSD1306_WHITE   = 1;
constexpr uint16_t SSD1306_INVERSE = 2;
constexpr uint8_t  SSD1306_SWITCHCAPVCC = 2;
constexpr uint8_t  SSD1306_EXTERNALVCC  = 1;

// Placeholder so "&Wire" compiles when passed into the constructor's TwoWire*
// slot -- Wire.<method>() calls are still handled by direct token replacement
// in preprocessor.cpp and never touch this; it exists purely so the
// address-of expression has a symbol to bind to.
struct VBWireStub {};
inline VBWireStub Wire;

// Compact 3x5 dot-matrix font -- deliberately smaller than a real display's
// glyphs (basic legibility, not pixel-fidelity to real hardware, same spirit
// as the MAX7219 dot grid and NeoPixel's plain circles elsewhere in this
// simulator). Each row is 3 bits, bit2..bit0 = left..right pixel. Lowercase
// maps onto the same glyphs as uppercase.
namespace vb_font3x5 {
struct Glyph { char c; uint8_t rows[5]; };
static const Glyph TABLE[] = {
    {' ', {0,0,0,0,0}},
    {'0', {7,5,5,5,7}}, {'1', {2,6,2,2,7}}, {'2', {7,1,7,4,7}},
    {'3', {7,1,7,1,7}}, {'4', {5,5,7,1,1}}, {'5', {7,4,7,1,7}},
    {'6', {7,4,7,5,7}}, {'7', {7,1,1,1,1}}, {'8', {7,5,7,5,7}},
    {'9', {7,5,7,1,7}},
    {'A', {2,5,7,5,5}}, {'B', {6,5,6,5,6}}, {'C', {3,4,4,4,3}},
    {'D', {6,5,5,5,6}}, {'E', {7,4,6,4,7}}, {'F', {7,4,6,4,4}},
    {'G', {3,4,5,5,3}}, {'H', {5,5,7,5,5}}, {'I', {7,2,2,2,7}},
    {'J', {1,1,1,5,2}}, {'K', {5,5,6,5,5}}, {'L', {4,4,4,4,7}},
    {'M', {5,7,7,5,5}}, {'N', {5,6,5,5,5}}, {'O', {2,5,5,5,2}},
    {'P', {6,5,6,4,4}}, {'Q', {2,5,5,2,1}}, {'R', {6,5,6,5,5}},
    {'S', {3,4,2,1,6}}, {'T', {7,2,2,2,2}}, {'U', {5,5,5,5,7}},
    {'V', {5,5,5,5,2}}, {'W', {5,5,5,7,5}}, {'X', {5,5,2,5,5}},
    {'Y', {5,5,2,2,2}}, {'Z', {7,1,2,4,7}},
    {'.', {0,0,0,0,2}}, {',', {0,0,0,2,4}}, {':', {0,2,0,2,0}},
    {';', {0,2,0,2,4}}, {'!', {2,2,2,0,2}}, {'?', {3,1,2,0,2}},
    {'-', {0,0,7,0,0}}, {'+', {0,2,7,2,0}}, {'=', {0,7,0,7,0}},
    {'(', {2,4,4,4,2}}, {')', {2,1,1,1,2}}, {'/', {1,1,2,4,4}},
    {'\'', {2,2,0,0,0}}, {'"', {5,5,0,0,0}}, {'%', {5,1,2,4,5}},
    {'*', {0,5,2,5,0}},
};

inline const uint8_t* lookup(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (const auto& g : TABLE)
        if (g.c == c) return g.rows;
    return nullptr; // unmapped char -- rendered blank
}
} // namespace vb_font3x5

// Shared in spirit with CircuitDetector::I2C_BUS_PIN_BASE (see
// src/core/circuit/i2cbus.h) -- the canvas key used when there's no real
// reset pin wired (rst_pin < 0, extremely common for I2C breakout modules
// with no dedicated RST line). Can't #include that header here (detector
// and this injected header compile completely separately), so this stays a
// hand-kept-in-sync base value -- but the *sequence* it produces is kept in
// sync automatically: global objects in one translation unit construct in
// declaration order, so this counter increments in the same order the
// detector's regex sees the matching Adafruit_SSD1306 constructors, and the
// two sides agree on 900, 901, 902... without either one being told the
// other's count.
static int g_next_i2c_bus_pin = 900;

class Adafruit_SSD1306 {
public:
    static constexpr int MAX_W = 128;
    static constexpr int MAX_H = 64;

    // Real starter-kit sketches declare this as a global, before vb_init()
    // sets `api` -- must not touch api-> here, same constraint every other
    // injected hardware class in this codebase has.
    Adafruit_SSD1306(uint8_t w, uint8_t h, void* twi = nullptr, int8_t rst_pin = -1,
                      uint32_t clkDuring = 400000UL, uint32_t clkAfter = 100000UL)
        : width_(w > MAX_W ? MAX_W : w), height_(h > MAX_H ? MAX_H : h),
          pinKey_(rst_pin >= 0 ? rst_pin : g_next_i2c_bus_pin++) {
        (void)twi; (void)clkDuring; (void)clkAfter;
        clearDisplay();
    }

    bool begin(uint8_t vccstate = SSD1306_SWITCHCAPVCC, uint8_t addr = 0x3C,
               bool reset = true, bool periphBegin = true) {
        (void)vccstate; (void)addr; (void)reset; (void)periphBegin;
        return true;
    }

    void clearDisplay() {
        for (int i = 0; i < MAX_W * MAX_H; ++i) buf_[i] = 0;
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
        uint8_t& px = buf_[y * MAX_W + x];
        if (color == SSD1306_INVERSE) px = px ? 0 : 1;
        else                          px = color ? 1 : 0;
    }

    void fillScreen(uint16_t color) {
        for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x)
                drawPixel(x, y, color);
    }

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
        int dx = x1 > x0 ? x1 - x0 : x0 - x1;
        int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            drawPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        drawLine(x, y, x + w - 1, y, color);
        drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
        drawLine(x, y, x, y + h - 1, color);
        drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        for (int16_t j = 0; j < h; ++j)
            drawLine(x, y + j, x + w - 1, y + j, color);
    }

    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
        int x = r, y = 0, err = 0;
        while (x >= y) {
            drawPixel(x0 + x, y0 + y, color); drawPixel(x0 + y, y0 + x, color);
            drawPixel(x0 - y, y0 + x, color); drawPixel(x0 - x, y0 + y, color);
            drawPixel(x0 - x, y0 - y, color); drawPixel(x0 - y, y0 - x, color);
            drawPixel(x0 + y, y0 - x, color); drawPixel(x0 + x, y0 - y, color);
            ++y;
            if (err <= 0) err += 2 * y + 1;
            if (err > 0) { --x; err -= 2 * x + 1; }
        }
    }

    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
        for (int16_t yy = -r; yy <= r; ++yy) {
            int16_t xx = (int16_t)(sqrt((double)(r * r - yy * yy)) + 0.5);
            drawLine(x0 - xx, y0 + yy, x0 + xx, y0 + yy, color);
        }
    }

    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
        int16_t byteWidth = (w + 7) / 8;
        for (int16_t j = 0; j < h; ++j)
            for (int16_t i = 0; i < w; ++i)
                if (bitmap[j * byteWidth + i / 8] & (0x80 >> (i & 7)))
                    drawPixel(x + i, y + j, color);
    }

    void setCursor(int16_t x, int16_t y) { cx_ = x; cy_ = y; }
    void setTextSize(uint8_t s) { textSize_ = s < 1 ? 1 : s; }
    void setTextColor(uint16_t c) { fg_ = c; bgSet_ = false; }
    void setTextColor(uint16_t c, uint16_t bg) { fg_ = c; bg_ = bg; bgSet_ = true; }
    void setTextWrap(bool w) { wrap_ = w; }
    void invertDisplay(bool) {}
    void dim(bool) {}

    size_t write(uint8_t c) {
        if (c == '\n') { cx_ = 0; cy_ += 6 * textSize_; return 1; }
        if (c == '\r') return 1;
        if (wrap_ && cx_ + 4 * textSize_ > width_) { cx_ = 0; cy_ += 6 * textSize_; }
        drawChar(cx_, cy_, (char)c);
        cx_ += 4 * textSize_;
        return 1;
    }

    void print(const char* s)      { while (s && *s) write((uint8_t)*s++); }
    void print(char c)             { write((uint8_t)c); }
    void print(const String& s)    { print(s.c_str()); }
    void print(int v)              { char b[16]; snprintf(b, sizeof(b), "%d", v);  print(b); }
    void print(unsigned int v)     { char b[16]; snprintf(b, sizeof(b), "%u", v);  print(b); }
    void print(long v)             { char b[24]; snprintf(b, sizeof(b), "%ld", v); print(b); }
    void print(unsigned long v)    { char b[24]; snprintf(b, sizeof(b), "%lu", v); print(b); }
    void print(double v, int digits = 2) { char b[32]; snprintf(b, sizeof(b), "%.*f", digits, v); print(b); }

    void println()                 { write('\n'); }
    void println(const char* s)    { print(s); write('\n'); }
    void println(char c)           { print(c); write('\n'); }
    void println(const String& s)  { print(s); write('\n'); }
    void println(int v)            { print(v); write('\n'); }
    void println(unsigned int v)   { print(v); write('\n'); }
    void println(long v)           { print(v); write('\n'); }
    void println(unsigned long v)  { print(v); write('\n'); }
    void println(double v, int digits = 2) { print(v, digits); write('\n'); }

    void display() {
        if (!api) return;
        api->oled_display(pinKey_, buf_, width_, height_);
    }

private:
    void drawChar(int16_t x, int16_t y, char c) {
        const uint8_t* rows = vb_font3x5::lookup(c);
        for (int ry = 0; ry < 5; ++ry) {
            uint8_t bits = rows ? rows[ry] : 0;
            for (int rx = 0; rx < 3; ++rx) {
                bool lit = (bits >> (2 - rx)) & 1;
                if (!lit && !bgSet_) continue;
                uint16_t c2 = lit ? fg_ : bg_;
                for (int sy = 0; sy < textSize_; ++sy)
                    for (int sx = 0; sx < textSize_; ++sx)
                        drawPixel(x + rx * textSize_ + sx, y + ry * textSize_ + sy, c2);
            }
        }
    }

    int width_, height_;
    int pinKey_;
    uint8_t buf_[MAX_W * MAX_H];

    int16_t cx_ = 0, cy_ = 0;
    uint8_t textSize_ = 1;
    uint16_t fg_ = SSD1306_WHITE, bg_ = SSD1306_BLACK;
    bool bgSet_ = false;
    bool wrap_ = true;
};
#endif
