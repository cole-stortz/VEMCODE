#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor IRSENSOR_ACTIVE  ("#dcc274");
static const QColor IRSENSOR_INACTIVE("#372e10");

class IRSensorItem : public ComponentItem {
    bool IRvalue_ = false;
public:
    IRSensorItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = IRvalue_ ? IRSENSOR_ACTIVE : IRSENSOR_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "IR Sensor");
    }

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        IRvalue_ = !IRvalue_;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, IRvalue_ ? 1 : 0);
    }

    void emitInitialValue() override {
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, IRvalue_ ? 1 : 0);
    }
};

static bool reg_ir_sensor = []() {
    ComponentDefinition def{
        "IR Sensor",
        {"IR", "IRSENSOR","IR_SENSOR", "IR_OUT", "INFRARED"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new IRSensorItem(pin, parent);
        }
    };
    def.wire_color = IRSENSOR_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();