#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QtMath>
#include <cmath>

static const QColor SERVO_FILL("#5ce0c2");

class ServoItem : public ComponentItem {
    int angle_;

public:
    ServoItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), angle_(90) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        QRectF body = r.adjusted(2, 2, -2, -2);
        p->setPen(QPen(SERVO_FILL.darker(180), 3));
        p->setBrush(SERVO_FILL.darker(400));
        p->drawRoundedRect(body, 4, 4);

        QPointF pivot(body.left() + 20, r.center().y());
        qreal hornLen = 16;

        p->setPen(QPen(QColor("#111"), 1));
        p->setBrush(QColor("#333"));
        p->drawEllipse(pivot, 6, 6);

        qreal a = qDegreesToRadians(angle_ - 90.0);
        QPointF tip = pivot + QPointF(std::cos(a) * hornLen, std::sin(a) * hornLen);
        p->setPen(QPen(SERVO_FILL, 3));
        p->drawLine(pivot, tip);
        p->setPen(Qt::NoPen);
        p->setBrush(SERVO_FILL);
        p->drawEllipse(tip, 4, 4);

        p->setPen(SERVO_FILL);
        p->setFont(QFont("Courier New", 8));
        qreal textX = pivot.x() + hornLen + 10;
        p->drawText(QRectF(textX, 0, r.width() - textX, r.height()), Qt::AlignCenter, QString("%1°").arg(angle_));

        // Straight lead on the left edge -- servos are outputs, so
        // CanvasWidget::updateWires attaches the wire at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(5, 15), QPointF(0, 15));
    }

    void onPinChanged(int, int value) override {
        angle_ = value;
        update();
    }
};

static bool registered = []() {
    ComponentDefinition def{
        "Servo",
        {"SERVO", "SRV"},
        {},    // detect_multi — none
        {".attach("},
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new ServoItem(pin, parent);
        }
    };
    def.wire_color = SERVO_FILL;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
