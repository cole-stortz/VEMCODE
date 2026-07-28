#ifndef VB_NEOPIXEL_H
#define VB_NEOPIXEL_H

// Color-order/bus-speed flags -- accepted for real-sketch source
// compatibility (constructor's 3rd arg, e.g. "NEO_GRB + NEO_KHZ800") but
// unused. The sim stores RGB triples directly and hands them straight to
// the canvas rather than serializing a WS2812 bitstream, so channel order
// and clock speed never matter here.
constexpr int NEO_RGB    = 0x00;
constexpr int NEO_GRB    = 0x01;
constexpr int NEO_RGBW   = 0x02;
constexpr int NEO_KHZ800 = 0x0000;
constexpr int NEO_KHZ400 = 0x0100;

class Adafruit_NeoPixel {
public:
    // Real starter-kit sketches declare this as a global, before vb_init()
    // sets `api` -- must not touch api-> here, same constraint LedControl's
    // constructor has.
    Adafruit_NeoPixel(uint16_t count, int16_t pin, int type = NEO_GRB + NEO_KHZ800)
        : pin_(pin), count_(clamp_(count)) {
        (void)type;
        clear();
    }

    void begin() {}

    void show() {
        if (!api) return;
        uint8_t buf[MAX_PIXELS * 3];
        for (int i = 0; i < count_; ++i) {
            buf[i * 3 + 0] = (uint8_t)(((int)r_[i] * brightness_) / 255);
            buf[i * 3 + 1] = (uint8_t)(((int)g_[i] * brightness_) / 255);
            buf[i * 3 + 2] = (uint8_t)(((int)b_[i] * brightness_) / 255);
        }
        api->neopixel_show(pin_, buf, count_);
    }

    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
        if (n >= (uint16_t)count_) return;
        r_[n] = r; g_[n] = g; b_[n] = b;
    }

    void setPixelColor(uint16_t n, uint32_t color) {
        setPixelColor(n, (uint8_t)(color >> 16), (uint8_t)(color >> 8), (uint8_t)color);
    }

    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    void clear() {
        for (int i = 0; i < MAX_PIXELS; ++i) r_[i] = g_[i] = b_[i] = 0;
    }

    void fill(uint32_t color = 0, uint16_t first = 0, uint16_t count = 0) {
        uint16_t last = count == 0 ? (uint16_t)count_ : (uint16_t)(first + count);
        if (last > (uint16_t)count_) last = (uint16_t)count_;
        for (uint16_t i = first; i < last; ++i) setPixelColor(i, color);
    }

    void setBrightness(uint8_t b) { brightness_ = b; }
    uint16_t numPixels() const { return (uint16_t)count_; }

private:
    static constexpr int MAX_PIXELS = 256;
    static int clamp_(int n) { return n < 1 ? 1 : (n > MAX_PIXELS ? MAX_PIXELS : n); }

    int pin_;
    int count_;
    int brightness_ = 255;
    uint8_t r_[MAX_PIXELS], g_[MAX_PIXELS], b_[MAX_PIXELS];
};
#endif
