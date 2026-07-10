#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor GENERIC_INPUT_ACTIVE  ("#748edc");
static const QColor GENERIC_INPUT_INACTIVE("#101a37");

class GenericInputItem : public ComponentItem {
    bool active_ = false;

public:
    GenericInputItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = active_ ? GENERIC_INPUT_ACTIVE : GENERIC_INPUT_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "Input");
    }

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        active_ = !active_;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, active_ ? 1 : 0);
    }

    void emitInitialValue() override {
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, active_ ? 1 : 0);
    }
};

// Never matched through the registry's keyword/pattern tiers -- CircuitDetector
// assigns this type directly as its final fallback when nothing else matches
// (see infer_type()), so detect_single/detect_multi/detect_pattern stay empty.
static bool registered = []() {
    ComponentDefinition def{
        "GenericInput",
        {}, {}, {},
        false, // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new GenericInputItem(pin, parent);
        }
    };
    def.wire_color = GENERIC_INPUT_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();