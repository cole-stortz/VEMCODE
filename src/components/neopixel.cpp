#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <algorithm>

static const QColor STRIP_BG ("#1a1a1a");
static const QColor STRIP_OFF("#2a2a2a");
static const QColor NEOPIXEL_ACCENT("#4ade80");

// Rendered as a grid of dots wrapping every COLS pixels within the standard
// 100px-wide footprint, growing downward as pixel count increases -- same
// "square footprint that stacks" shape MAX7219 uses for its device chain.
class NeoPixelItem : public ComponentItem {
    static constexpr int MAX_PIXELS = 256;
    static constexpr int COLS = 10;
    int pixelCount_ = 1;
    QColor pixels_[MAX_PIXELS];

public:
    NeoPixelItem(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {
        std::fill(std::begin(pixels_), std::end(pixels_), STRIP_OFF);
    }

    void configureStripLength(int count) override {
        pixelCount_ = count < 1 ? 1 : (count > MAX_PIXELS ? MAX_PIXELS : count);
    }

    QRectF boundingRect() const override {
        int rows = (pixelCount_ + COLS - 1) / COLS;
        return QRectF(0, 0, 100, 16.0 * rows + 8.0);
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF box = boundingRect();
        p->setPen(QPen(QColor("#000000"), 3));
        p->setBrush(STRIP_BG);
        p->drawRect(box);

        constexpr float MARGIN = 8.0f;
        float cell = (100.0f - 2.0f * MARGIN) / COLS;
        float dotD = cell * 0.7f;

        p->setPen(Qt::NoPen);
        for (int i = 0; i < pixelCount_; ++i) {
            int row = i / COLS;
            int col = i % COLS;
            float cx = MARGIN + col * cell + cell / 2.0f;
            float cy = MARGIN + row * cell + cell / 2.0f;
            p->setBrush(pixels_[i]);
            p->drawEllipse(QPointF(cx, cy), dotD / 2.0f, dotD / 2.0f);
        }

        // Straight lead on the left edge -- NeoPixel is an output, so
        // CanvasWidget::updateWires attaches the wire at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(5, 15), QPointF(0, 15));
    }

    // Whole-strip update, sent once per show() -- rgb is pixelCount_*3
    // interleaved bytes, same order the runtime hook packs them in.
    void updateStripPixels(const QByteArray& rgb) override {
        int n = std::min(pixelCount_, (int)(rgb.size() / 3));
        for (int i = 0; i < n; ++i) {
            auto r = (uchar)rgb[i * 3 + 0];
            auto g = (uchar)rgb[i * 3 + 1];
            auto b = (uchar)rgb[i * 3 + 2];
            pixels_[i] = QColor(r, g, b);
        }
        update();
    }
};

static bool registered_neopixel = []() {
    ComponentDefinition def{
        "NeoPixel",
        {"NEOPIXEL", "WS2812", "PIXELS", "PIXEL", "STRIP"},
        {},    // detect_multi -- none, single-pin component
        {},    // detect_pattern -- none, handled by CircuitDetector::detect_neopixel
               // (constructor's 1st arg is LED count, not a keyword-matched pin)
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new NeoPixelItem(pin, parent);
        }
    };
    def.wire_color = NEOPIXEL_ACCENT;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
