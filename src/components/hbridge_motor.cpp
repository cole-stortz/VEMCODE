#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QRadialGradient>

static const QColor HBRIDGE_ACTIVE("#dc7474");

class HBridgeMotorItem : public ComponentItem {
    int pwmPin_;
    int cwisePin_ = -1;
    int antiCwisePin_ = -1;

    int pwm_ = 0;
    bool cwise_ = false;
    bool antiCwise_ = false;

public:
    HBridgeMotorItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), pwmPin_(pin) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 54); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QString dir = (cwise_ && antiCwise_) ? "BRAKE"
                    : cwise_                 ? "CW"
                    : antiCwise_              ? "CCW"
                    :                           "STOP";

        QRectF r = boundingRect();
        QPointF c(r.left() + r.width() * 0.32, r.center().y() - 4);
        qreal rad = qMin(r.width(), r.height()) * 0.28;
        QColor body = QColor("#888").lighter(pwm_ > 0 ? 110 : 80);
        QRadialGradient g(c - QPointF(rad * 0.3, rad * 0.3), rad * 1.6);
        g.setColorAt(0.0, body.lighter(140));
        g.setColorAt(1.0, body.darker(140));
        p->setPen(QPen(QColor("#333"), 1));
        p->setBrush(g);
        p->drawEllipse(c, rad, rad);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#555"));
        p->drawRect(QRectF(c.x() + rad * 0.7, c.y() - 3, rad * 0.6, 6));

        if (dir == "CW" || dir == "CCW") {
            p->setPen(QPen(HBRIDGE_ACTIVE, 2));
            QRectF arcRect(c.x() - rad * 0.7, c.y() - rad * 0.7, rad * 1.4, rad * 1.4);
            int span = dir == "CW" ? -270 * 16 : 270 * 16;
            p->drawArc(arcRect, 0, span);
        }

        p->setPen(HBRIDGE_ACTIVE);
        p->setFont(QFont("Courier New", 7));
        p->drawText(QRectF(r.left(), r.bottom() - 16, r.width(), 16), Qt::AlignCenter,
                    QString("%1 pwm:%2").arg(dir).arg(pwm_));

        // Straight leads on the left edge, one per PWM/ANTI_CWISE/CWISE pin
        // slot -- h-bridge motors are outputs, so CanvasWidget::updateWires
        // attaches wire i at local (0, 15 + i*5), same spacing as WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 3; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(10, ly), QPointF(0, ly));
        }
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) antiCwisePin_ = pins[1];
        if (pins.size() > 2) cwisePin_ = pins[2];
    }

    void onPinChanged(int pin, int value) override {
        if (pin == pwmPin_)            pwm_ = value;
        else if (pin == cwisePin_)     cwise_ = value > 0;
        else if (pin == antiCwisePin_) antiCwise_ = value > 0;
        update();
    }
};

static bool registered = []() {
    ComponentDefinition def{
        "HBridgeMotor",
        {"MOTOR", "HBRIDGE", "ENA", "IN1"},
        {
            {"PWM",        {"PWM"}},
            {"ANTI_CWISE", {"ANTI"}},
            {"CWISE",      {"CWISE", "CW", "DIR"}},
        },
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new HBridgeMotorItem(pin, parent);
        },
        MultiPinStrategy::Prefix,
        "PWM"
    };
    def.wire_color = HBRIDGE_ACTIVE;
    ComponentRegistry::instance().register_component(def);

    // Separate entry for the bare ENA/IN1/IN2 wiring style (L298N/L293D
    // naming with no shared prefix) -- detect_prefix_group requires an
    // underscore to derive a group key, so this never grouped. Same
    // type_name/create_item, same tradeoff Stepper's IN1-IN4 entry accepts.
    ComponentDefinition enaIn{
        "HBridgeMotor",
        {},
        {
            {"PWM",        {"ENA"}},
            {"ANTI_CWISE", {"IN2"}},
            {"CWISE",      {"IN1"}},
        },
        {},
        true,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new HBridgeMotorItem(pin, parent);
        },
        MultiPinStrategy::Singleton,
        "PWM"
    };
    enaIn.wire_color = HBRIDGE_ACTIVE;
    ComponentRegistry::instance().register_component(enaIn);
    return true;
}();