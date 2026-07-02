#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor LED_ACTIVE  ("#ffdd44");
static const QColor LED_INACTIVE("#3a3000");

class LedItem : public ComponentItem {
    bool active_;

public:
    LedItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), active_(false) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = active_ ? LED_ACTIVE : LED_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "LED");
    }

    void onPinChanged(int, int value) override {
        active_ = (value > 0);
        update();
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "LED",
        {"LED", "LAMP", "DIODE", "INDICATOR"},
        {},    // detect_multi — none
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new LedItem(pin, parent);
        }
    });
    return true;
}();
