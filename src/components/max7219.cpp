#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor MATRIX_BG    ("#1a1a1a");
static const QColor MATRIX_LIT   ("#dc4a4a");
static const QColor MATRIX_UNLIT ("#3a1414");

// Single device: a square whose side length matches the long side (100px)
// every other component's rectangle uses -- an 8x8 dot grid reads better in
// a square than squeezed into the standard 100x44 rectangle. Daisy-chained
// devices (LedControl's 4th constructor arg) stack as additional 100x100
// bands, tallest at the bottom of the chain the same order LedControl
// addresses them (addr 0 on top).
class Max7219Item : public ComponentItem {
    static constexpr int MAX_DEVICES = 8;
    int clkPin_ = -1;
    int dinPin_ = -1;
    int numDevices_ = 1;
    bool lit_[MAX_DEVICES][8][8] = {};

public:
    Max7219Item(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 100.0 * numDevices_); }

    void configureDeviceCount(int count) override {
        numDevices_ = count < 1 ? 1 : (count > MAX_DEVICES ? MAX_DEVICES : count);
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        constexpr int   N      = 8;
        constexpr float MARGIN = 8.0f;
        float cell = (100.0f - 2.0f * MARGIN) / N;
        float dotD = cell * 0.7f;

        for (int dev = 0; dev < numDevices_; ++dev) {
            float bandY = dev * 100.0f;

            p->setPen(QPen(QColor("#000000"), 1));
            p->setBrush(MATRIX_BG);
            p->drawRect(QRectF(0, bandY, 100, 100));

            p->setPen(Qt::NoPen);
            for (int row = 0; row < N; ++row) {
                for (int col = 0; col < N; ++col) {
                    p->setBrush(lit_[dev][row][col] ? MATRIX_LIT : MATRIX_UNLIT);
                    float cx = MARGIN + col * cell + cell / 2.0f;
                    float cy = bandY + MARGIN + row * cell + cell / 2.0f;
                    p->drawEllipse(QPointF(cx, cy), dotD / 2.0f, dotD / 2.0f);
                }
            }
        }
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) clkPin_ = pins[1];
        if (pins.size() > 2) dinPin_ = pins[2];
    }

    // Row updates arrive keyed by this item's own pin (CS) once LedControl.h
    // has fully latched a row byte -- CLK/DIN never toggle a visible state on
    // their own, so there's no per-bit GUI-thread sampling to worry about
    // (the same cross-thread scanning limit that ruled out multi-digit
    // seven-segment multiplexing never comes up here).
    void updateMatrixRow(int addr, int row, int bits) override {
        if (addr < 0 || addr >= numDevices_ || row < 0 || row > 7) return;
        for (int col = 0; col < 8; ++col)
            lit_[addr][row][col] = (bits >> (7 - col)) & 1;
        update();
    }
};

static bool registered_max7219 = []() {
    // MAX_CS / MAX_CLK / MAX_DIN style naming -- shared prefix.
    ComponentDefinition prefixed{
        "Max7219",
        {},
        {
            {"CS",  {"CS"}},
            {"CLK", {"CLK"}},
            {"DIN", {"DIN"}},
        },
        {},
        true, // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new Max7219Item(pin, parent);
        },
        MultiPinStrategy::Prefix,
        "CS"
    };
    prefixed.wire_color = MATRIX_LIT;
    ComponentRegistry::instance().register_component(prefixed);

    // Bare CS/CLK/DIN defines with no shared prefix -- same tradeoff
    // HBridgeMotor's bare ENA/IN1/IN2 entry accepts.
    ComponentDefinition bare{
        "Max7219",
        {},
        {
            {"CS",  {"CS"}},
            {"CLK", {"CLK"}},
            {"DIN", {"DIN"}},
        },
        {},
        true,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new Max7219Item(pin, parent);
        },
        MultiPinStrategy::Singleton,
        "CS"
    };
    bare.wire_color = MATRIX_LIT;
    ComponentRegistry::instance().register_component(bare);
    return true;
}();
