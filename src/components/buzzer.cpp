#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor BUZZER_ACTIVE  ("#dc9b74");
static const QColor BUZZER_INACTIVE("#371f10");

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
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "Buzzer");
    }

    void onPinChanged(int, int value) override {
        active_ = (value > 0);
        update();
    }
};

static bool registered = []() {
    ComponentDefinition def{
        "Buzzer",
        {"BUZZER", "BUZZ", "SPEAKER", "TONE", "PIEZO"},
        {},    // detect_multi — none
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new BuzzerItem(pin, parent);
        }
    };
    def.wire_color = BUZZER_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();