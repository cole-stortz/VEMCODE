#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor BUZZER_ACTIVE  ("#db7d25");
static const QColor BUZZER_INACTIVE("#401f01");

class BuzzerItem : public ComponentItem {
    bool active_;

public:
    BuzzerItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), active_(false) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = active_ ? BUZZER_ACTIVE : BUZZER_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "Buzzer");
    }

    void onPinChanged(int, int value) override {
        active_ = (value > 0);
        update();
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "Buzzer",
        {"BUZZER", "BUZZ", "SPEAKER", "TONE", "PIEZO"},
        {},    // detect_multi — none
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new BuzzerItem(pin, parent);
        }
    });
    return true;
}();