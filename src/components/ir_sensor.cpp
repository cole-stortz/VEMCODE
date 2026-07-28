#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QRadialGradient>
#include <QCursor>

static const QColor IRSENSOR_ACTIVE("#dcc274");

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
        p->setPen(QPen(QColor("#0f0a05"), 1));
        p->setBrush(QColor("#241a0f"));
        p->drawRoundedRect(r, 4, 4);

        QPointF ledC(r.left() + r.width() * 0.28, r.center().y() - 2);
        qreal ledR = qMin(r.width(), r.height()) * 0.16;
        QRadialGradient g(ledC, ledR * 1.4);
        g.setColorAt(0.0, QColor(90, 60, 160, 200));
        g.setColorAt(1.0, QColor(40, 20, 80, 220));
        p->setPen(QPen(QColor("#000"), 1));
        p->setBrush(g);
        p->drawEllipse(ledC, ledR, ledR);

        QRectF trim(r.center().x() - 6, r.top() + 6, 16, 16);
        p->setPen(QPen(QColor("#1a3a6e"), 1));
        p->setBrush(QColor("#2a5aa0"));
        p->drawRect(trim);
        p->setPen(QPen(QColor("#0a1a3a"), 1.5));
        p->drawLine(trim.center() + QPointF(-5, -3), trim.center() + QPointF(5, 3));

        QPointF ind(r.right() - r.width() * 0.18, r.center().y() - 2);
        p->setPen(Qt::NoPen);
        p->setBrush(IRvalue_ ? IRSENSOR_ACTIVE : QColor("#372e10"));
        p->drawEllipse(ind, 4, 4);

        p->setPen(IRSENSOR_ACTIVE);
        p->setFont(QFont("Courier New", 7));
        p->drawText(r, Qt::AlignHCenter | Qt::AlignBottom, "IR");

        // Straight lead on the right edge -- IR sensors are inputs, so
        // CanvasWidget::updateWires attaches the wire at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 10, 15), QPointF(r.width(), 15));
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