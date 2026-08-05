#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor IRSENSOR_ACTIVE("#d1a52e");

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
        QRectF r = boundingRect();
        bool dark = isDarkTheme();
        p->setPen(QPen(IRSENSOR_ACTIVE.darker(180), 3));
        p->setBrush(themedHousing(IRSENSOR_ACTIVE, dark, 400));
        p->drawRoundedRect(r, 4, 4);

        QPointF ledC(r.left() + r.width() * 0.28, r.center().y() - 2);
        qreal ledR = qMin(r.width(), r.height()) * 0.3;
        QColor ledColor("#5a3ca0");
        p->setPen(QPen(ledColor.darker(180), 2));
        p->setBrush(ledColor);
        p->drawEllipse(ledC, ledR, ledR);

        QPointF ind(r.right() - r.width() * 0.25, r.center().y() - 2);
        QColor indColor = IRvalue_ ? QColor("#4ec94e") : themedHousing(IRSENSOR_ACTIVE, dark, 400);
        p->setPen(QPen(indColor.darker(180), 1.5));
        p->setBrush(indColor);
        p->drawEllipse(ind, 6, 6);

        p->setPen(IRSENSOR_ACTIVE);
        p->setFont(QFont("Courier New", 7));
        p->drawText(r, Qt::AlignHCenter | Qt::AlignBottom, "IR");

        // Straight lead on the right edge; wire attaches at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 5, 15), QPointF(r.width(), 15));
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