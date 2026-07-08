#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor IRSENSOR_ACTIVE  ("#f48d9b");
static const QColor IRSENSOR_INACTIVE("#642e36");

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
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "IR Sensor");
    }

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        IRvalue_ = !IRvalue_;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, IRvalue_ ? 1 : 0);
    }
};

static bool reg_ir_sensor = []() {
    ComponentRegistry::instance().register_component({
        "IR Sensor",
        {"IR", "IRSENSOR","IR_SENSOR", "IR_OUT", "INFRARED"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new IRSensorItem(pin, parent);
        }
    });
    return true;
}();