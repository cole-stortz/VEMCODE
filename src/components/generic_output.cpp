#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor GENERIC_ACTIVE  ("#aaaaaa");
static const QColor GENERIC_INACTIVE("#2a2a2a");

class GenericOutputItem : public ComponentItem {
    bool active_ = false;

public:
    GenericOutputItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = active_ ? GENERIC_ACTIVE : GENERIC_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "Output");
    }

    void onPinChanged(int, int value) override {
        active_ = (value > 0);
        update();
    }
};

// Never matched through the registry's keyword/pattern tiers -- CircuitDetector
// assigns this type directly as its final fallback when nothing else matches
// (see infer_type()), so detect_single/detect_multi/detect_pattern stay empty.
static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "GenericOutput",
        {}, {}, {},
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new GenericOutputItem(pin, parent);
        }
    });
    return true;
}();