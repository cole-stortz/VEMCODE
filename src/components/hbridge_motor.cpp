#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QPainterPath>
#include <QLineF>

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
        QRectF r = boundingRect();

        // Leads drawn under the body -- the body paints over the overlap, so
        // the visible stubs always end up flush with the body's edge without
        // having to keep the two in sync.
        // H-bridge motors are outputs, so CanvasWidget::updateWires attaches
        // wire i at local (0, 15 + i*5), same spacing as WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 3; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(10, ly), QPointF(0, ly));
        }

        QString dir = (cwise_ && antiCwise_) ? "BRAKE"
                    : cwise_                 ? "CW"
                    : antiCwise_              ? "CCW"
                    :                           "STOP";

        qreal rad = r.height() * 0.42;
        QPointF c(r.left() + rad + 6, r.center().y());
        QColor body = QColor("#888").lighter(pwm_ > 0 ? 110 : 80);
        p->setPen(QPen(QColor("#333"), 3));
        p->setBrush(body);
        p->drawEllipse(c, rad, rad);

        p->setPen(QPen(QColor("#222"), 1));
        p->setBrush(QColor("#555"));
        p->drawEllipse(c, rad * 0.18, rad * 0.18);

        if (dir == "CW" || dir == "CCW") {
            p->setPen(QPen(HBRIDGE_ACTIVE, 2));
            QRectF arcRect(c.x() - rad * 0.7, c.y() - rad * 0.7, rad * 1.4, rad * 1.4);
            qreal spanDeg = dir == "CW" ? -270 : 270;
            p->drawArc(arcRect, 0, spanDeg * 16);

            // Arrowhead at the arc's terminal end -- sampled via arcMoveTo so
            // the tangent direction always matches whatever drawArc actually
            // rendered, rather than hand-deriving trig signs for Qt's
            // arc-angle convention.
            QPainterPath tipPath, backPath;
            tipPath.arcMoveTo(arcRect, spanDeg);
            backPath.arcMoveTo(arcRect, spanDeg - (spanDeg > 0 ? 15 : -15));
            QPointF tip = tipPath.currentPosition();
            QLineF dirLine(backPath.currentPosition(), tip);
            dirLine.setLength(1);
            QPointF dirVec = dirLine.p2() - dirLine.p1();
            QPointF normVec(-dirVec.y(), dirVec.x());
            qreal arrowLen = 6;
            QPolygonF arrow;
            arrow << tip + dirVec * arrowLen
                  << tip + normVec * (arrowLen * 0.55) - dirVec * (arrowLen * 0.2)
                  << tip - normVec * (arrowLen * 0.55) - dirVec * (arrowLen * 0.2);
            p->setPen(Qt::NoPen);
            p->setBrush(HBRIDGE_ACTIVE);
            p->drawPolygon(arrow);
        }

        p->setPen(HBRIDGE_ACTIVE);
        p->setFont(QFont("Courier New", 7));
        qreal textX = r.width() * 0.6;
        qreal textW = r.width() - textX;
        p->drawText(QRectF(textX, r.center().y() - 14, textW, 14), Qt::AlignCenter, dir);
        p->drawText(QRectF(textX, r.center().y(), textW, 14), Qt::AlignCenter, QString("pwm:%1").arg(pwm_));
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