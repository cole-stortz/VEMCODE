#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor HBRIDGE_ACTIVE  ("#ff44aa");
static const QColor HBRIDGE_INACTIVE("#3a0020");

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

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        bool active = (cwise_ || antiCwise_) && pwm_ > 0;
        QColor fill = active ? HBRIDGE_ACTIVE : HBRIDGE_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));

        QString dir = (cwise_ && antiCwise_) ? "BRAKE"
                    : cwise_                 ? "CW"
                    : antiCwise_              ? "CCW"
                    :                           "STOP";
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft,
                    dir + QString("\nPWM: %1").arg(pwm_));
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) cwisePin_ = pins[1];
        if (pins.size() > 2) antiCwisePin_ = pins[2];
    }

    void onPinChanged(int pin, int value) override {
        if (pin == pwmPin_)            pwm_ = value;
        else if (pin == cwisePin_)     cwise_ = value > 0;
        else if (pin == antiCwisePin_) antiCwise_ = value > 0;
        update();
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "HBridgeMotor",
        {"MOTOR", "HBRIDGE", "ENA", "IN1"},
        {
            {"PWM",        {"PWM"}},
            {"CWISE",      {"CWISE", "CW", "DIR"}},
            {"ANTI_CWISE", {"ANTI"}},
        },
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new HBridgeMotorItem(pin, parent);
        }
    });
    return true;
}();