#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor SEVENSEG_FILL ("#1a1a1a");
static const QColor SEVENSEG_DIGIT("#dc4a4a");

// Renders the lit segment pattern as the actual digit character (e.g. "7")
// rather than drawing 7 individual segment rectangles -- much simpler, and
// still tells you at a glance what the sketch thinks it's displaying.
// Single-digit only: a multiplexed multi-digit display strobes its
// digit-select pins far faster than the GUI thread can sample (same
// cross-thread scanning limitation found with the keypad matrix attempt).
class SevenSegItem : public ComponentItem {
    int  segPins_[7]  = {-1, -1, -1, -1, -1, -1, -1}; // A..G
    bool segState_[7] = {false, false, false, false, false, false, false};

public:
    SevenSegItem(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 54); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        p->setPen(QPen(QColor("#000000"), 1));
        p->setBrush(SEVENSEG_FILL);
        p->drawRect(boundingRect());
        p->setPen(SEVENSEG_DIGIT);
        p->setFont(QFont("Courier New", 28, QFont::Bold));
        p->drawText(boundingRect(), Qt::AlignCenter, QString(decodeDigit()));
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        for (size_t i = 0; i < pins.size() && i < 7; ++i)
            segPins_[i] = pins[i];
    }

    void onPinChanged(int pin, int value) override {
        for (int i = 0; i < 7; ++i) {
            if (segPins_[i] == pin) {
                segState_[i] = value != 0;
                update();
                return;
            }
        }
    }

private:
    // Bit order A=0 B=1 C=2 D=3 E=4 F=5 G=6, HIGH = segment lit. Assumes
    // common-cathode wiring; a common-anode sketch (segment ON = LOW) will
    // render inverted.
    QChar decodeDigit() const {
        uint8_t mask = 0;
        for (int i = 0; i < 7; ++i)
            if (segState_[i]) mask |= (1 << i);

        switch (mask) {
            case 0b0111111: return '0';
            case 0b0000110: return '1';
            case 0b1011011: return '2';
            case 0b1001111: return '3';
            case 0b1100110: return '4';
            case 0b1101101: return '5';
            case 0b1111101: return '6';
            case 0b0000111: return '7';
            case 0b1111111: return '8';
            case 0b1101111: return '9';
            case 0b1000000: return '-';
            case 0:         return ' ';
            default:        return '?';
        }
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
