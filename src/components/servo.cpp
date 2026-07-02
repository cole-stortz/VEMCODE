#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor SERVO_FILL("#0c9520");

class ServoItem : public ComponentItem {
    int angle_;

public:
    ServoItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), angle_(90) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = SERVO_FILL;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, QString("Servo: %1°").arg(angle_));
    }

    void onPinChanged(int, int value) override {
        angle_ = value;
        update();
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "Servo",
        {"SERVO", "SRV"},
        {},    // detect_multi — none
        {".attach("},
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new ServoItem(pin, parent);
        }
    });
    return true;
}();
