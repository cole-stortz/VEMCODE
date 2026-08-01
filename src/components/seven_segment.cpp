#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor SEVENSEG_DIGIT("#d13a5c");

// Renders the lit segment pattern as the actual digit character (e.g. "7")
// rather than drawing 7 individual segment rectangles -- much simpler, and
// still tells you at a glance what the sketch thinks it's displaying.
// Single-digit only: a multiplexed multi-digit display strobes its
// digit-select pins far faster than the GUI thread can sample (same
// cross-thread scanning limitation found with the keypad matrix attempt).
class SevenSegItem : public ComponentItem {
    int  segPins_[7]  = {-1, -1, -1, -1, -1, -1, -1}; // A..G
    bool segState_[7] = {false, false, false, false, false, false, false};
    bool commonAnode_ = false;

public:
    SevenSegItem(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 54); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        p->setPen(QPen(SEVENSEG_DIGIT.darker(180), 3));
        p->setBrush(isDarkTheme() ? SEVENSEG_DIGIT.darker(400) : SEVENSEG_DIGIT.darker(175));
        p->drawRect(r);

        QRectF area = r.adjusted(r.width() * 0.3, r.height() * 0.12, -r.width() * 0.3, -r.height() * 0.12);
        qreal w = area.width(), h = area.height();
        qreal t = w * 0.28;
        qreal x = area.left(), y = area.top();
        QColor unlit = SEVENSEG_DIGIT.darker(600);

        auto drawSeg = [&](const QRectF& rect, bool on) {
            p->setPen(Qt::NoPen);
            p->setBrush(on ? SEVENSEG_DIGIT : unlit);
            p->drawRoundedRect(rect, t * 0.35, t * 0.35);
        };

        qreal midY = y + h / 2 - t / 2;
        drawSeg(QRectF(x + t * 0.3, y, w - t * 0.6, t * 0.6), segState_[0]);                          // A top
        drawSeg(QRectF(x + w - t * 0.6, y + t * 0.2, t * 0.6, h / 2 - t * 0.4), segState_[1]);         // B top-right
        drawSeg(QRectF(x + w - t * 0.6, y + h / 2 + t * 0.2, t * 0.6, h / 2 - t * 0.4), segState_[2]); // C bottom-right
        drawSeg(QRectF(x + t * 0.3, y + h - t * 0.6, w - t * 0.6, t * 0.6), segState_[3]);             // D bottom
        drawSeg(QRectF(x, y + h / 2 + t * 0.2, t * 0.6, h / 2 - t * 0.4), segState_[4]);               // E bottom-left
        drawSeg(QRectF(x, y + t * 0.2, t * 0.6, h / 2 - t * 0.4), segState_[5]);                       // F top-left
        drawSeg(QRectF(x + t * 0.3, midY, w - t * 0.6, t * 0.6), segState_[6]);                        // G middle

        // Straight leads on the left edge, one per A-G segment pin slot --
        // seven-segment displays are outputs, so CanvasWidget::updateWires
        // attaches wire i at local (0, 15 + i*5), same spacing as WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 7; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(10, ly), QPointF(0, ly));
        }
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        for (size_t i = 0; i < pins.size() && i < 7; ++i)
            segPins_[i] = pins[i];
    }

    void onPinChanged(int pin, int value) override {
        for (int i = 0; i < 7; ++i) {
            if (segPins_[i] == pin) {
                bool raw = value != 0;
                segState_[i] = commonAnode_ ? !raw : raw; // anode: LOW == lit
                update();
                return;
            }
        }
    }

    bool supportsPolarity() const override { return true; }
    bool isCommonAnode() const override { return commonAnode_; }
    void configurePolarity(bool commonAnode) override {
        commonAnode_ = commonAnode;
        update();
    }
};

static bool registered_sevenseg = []() {
    ComponentDefinition def{
        "SevenSegment",
        {},
        {
            {"A", {"SEG_A", "SEGA"}},
            {"B", {"SEG_B", "SEGB"}},
            {"C", {"SEG_C", "SEGC"}},
            {"D", {"SEG_D", "SEGD"}},
            {"E", {"SEG_E", "SEGE"}},
            {"F", {"SEG_F", "SEGF"}},
            {"G", {"SEG_G", "SEGG"}},
        },
        {},
        true, // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new SevenSegItem(pin, parent);
        },
        MultiPinStrategy::Singleton,
        "A"
    };
    def.wire_color = SEVENSEG_DIGIT;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
