#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLinearGradient>
#include <QtMath>
#include <cmath>

static const QColor SERVO_FILL("#74dc8e");

class ServoItem : public ComponentItem {
    int angle_;

public:
    ServoItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), angle_(90) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        QRectF body = r.adjusted(r.width() * 0.15, r.height() * 0.3, -r.width() * 0.15, -6);

        QColor housing("#2a2a2a");
        QLinearGradient bg(body.topLeft(), body.bottomLeft());
        bg.setColorAt(0.0, housing.lighter(130));
        bg.setColorAt(0.5, housing);
        bg.setColorAt(1.0, housing.darker(120));
        p->setPen(QPen(housing.darker(180), 1.2));
        p->setBrush(bg);
        p->drawRoundedRect(body, 4, 4);

        QPointF pivot(body.center().x(), body.top());
        p->setPen(QPen(QColor("#111"), 1));
        p->setBrush(QColor("#555"));
        p->drawEllipse(pivot, 5, 5);

        qreal a = qDegreesToRadians(angle_ - 90.0);
        QPointF tip = pivot + QPointF(std::cos(a) * body.width() * 0.55, std::sin(a) * body.width() * 0.55);
        p->setPen(QPen(SERVO_FILL, 3));
        p->drawLine(pivot, tip);
        p->setPen(Qt::NoPen);
        p->setBrush(SERVO_FILL);
        p->drawEllipse(tip, 4, 4);

        p->setPen(QColor("#cfcfcf"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(r, Qt::AlignHCenter | Qt::AlignBottom, QString("%1°").arg(angle_));

        // Straight lead on the left edge -- servos are outputs, so
        // CanvasWidget::updateWires attaches the wire at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(10, 15), QPointF(0, 15));
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
