#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor HBRIDGE_ACTIVE  ("#dc7474");
static const QColor HBRIDGE_INACTIVE("#371010");

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
        bool active = (cwise_ || antiCwise_) && pwm_ > 0;
        QColor fill = active ? HBRIDGE_ACTIVE : HBRIDGE_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));

        QString dir = (cwise_ && antiCwise_) ? "BRAKE"
                    : cwise_                 ? "CW"
                    : antiCwise_              ? "CCW"
                    :                           "STOP";
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft,
                    dir + QString("\nPWM: %1").arg(pwm_));
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