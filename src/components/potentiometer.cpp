#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <cmath>

static const QColor POT_ACTIVE("#7a7a2e");

class PotItem : public ComponentItem {
    bool dragging_ = false;
    int value_ = 512;
    int dragStartY_ = 0;
    int dragStartValue_ = 512;

public:
    PotItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::SizeVerCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();

        // Lead drawn under the knob, extending well past its edge -- the
        // knob paints over the overlap, so the visible stub always ends up
        // flush with the knob regardless of the exact radius.
        // Potentiometers are inputs, so CanvasWidget::updateWires attaches
        // the wire at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 50, 15), QPointF(r.width(), 15));

        qreal ratio = value_ / 1023.0;

        qreal rad = qMin(r.width(), r.height()) * 0.45;
        QPointF c(r.center().x() + 10, r.center().y());
        QColor knobColor = POT_ACTIVE.lighter(int(100 + ratio * 40));
        p->setPen(QPen(knobColor.darker(180), 3));
        p->setBrush(knobColor);
        p->drawEllipse(c, rad, rad);

        p->setPen(QPen(QColor("#111"), 1));
        for (int i = 0; i < 11; ++i) {
            qreal a = qDegreesToRadians(-135.0 + i * 27.0);
            QPointF p1(c.x() + std::cos(a) * rad * 0.75, c.y() + std::sin(a) * rad * 0.75);
            QPointF p2(c.x() + std::cos(a) * rad * 0.95, c.y() + std::sin(a) * rad * 0.95);
            p->drawLine(p1, p2);
        }

        qreal ang = qDegreesToRadians(-135.0 + ratio * 270.0);
        p->setPen(QPen(QColor("#1a1a1a"), 2));
        p->drawLine(c, c + QPointF(std::cos(ang) * rad * 0.8, std::sin(ang) * rad * 0.8));

        p->setPen(POT_ACTIVE);
        p->setFont(QFont("Courier New", 7));
        qreal textW = r.width() * 0.4;
        p->drawText(QRectF(0, 0, textW, r.height()), Qt::AlignCenter, QString::number(value_));
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        dragging_ = true;
        dragStartY_ = event->pos().y();
        dragStartValue_ = value_;
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (!dragging_) return;
        int delta = dragStartY_ - event->pos().y();
        value_ = qBound(0, dragStartValue_ + delta * 4, 1023);
        update();
        emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, value_);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override {
        dragging_ = false;
    }

    void emitInitialValue() override {
        emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, value_);
    }
};

static bool reg_pot = []() {
    ComponentDefinition def{
        "Potentiometer",
        {"POT", "POTENTIOMETER", "KNOB", "DIAL"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new PotItem(pin, parent);
        }
    };
    def.wire_color = POT_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();