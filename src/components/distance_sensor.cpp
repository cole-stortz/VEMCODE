#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <cmath>

static const QColor DISTANCE_SENSOR_FILL("#003a3a");

class DistanceSensorItem : public ComponentItem {
public:
    DistanceSensorItem(int p, QGraphicsItem* parent)
        : ComponentItem(p, parent) {
        auto* input = new QLineEdit("10");
        input->setFixedSize(60, 20);
        input->setPlaceholderText("cm");
        input->setStyleSheet("background:#001a1a; color:#44ffff; border:1px solid #44ffff;");
        auto* proxy = new QGraphicsProxyWidget(this);
        proxy->setWidget(input);
        proxy->setPos(34, 18);
        connect(input, &QLineEdit::textChanged, this, [this](const QString& text) {
            bool ok;
            float cm = text.toFloat(&ok);
            if (ok && cm >= 0) {
                qulonglong micros = (qulonglong)std::ceil(cm * 2.0f / 0.034f);
                emit inputChanged(pin(), (int)ComponentEventType::PulseUs, micros);
            }
        });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = DISTANCE_SENSOR_FILL;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#44ffff"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 16), Qt::AlignLeft, "Distance");
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "DistanceSensor",
        {"TRIG", "ECHO", "DISTANCE", "ULTRASONIC", "SONAR", "HCSR"},
        {},
        {"pulseIn("},
        false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new DistanceSensorItem(pin, parent);
        }
    });
    return true;
}();