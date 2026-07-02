#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <cmath>

static const QColor DISTANCE_SENSOR_FILL("#003a3a");

class DistanceSensorItem : public ComponentItem {
    QLineEdit* input_;

public:
    DistanceSensorItem(int p, QGraphicsItem* parent)
        : ComponentItem(p, parent) {
        input_ = new QLineEdit();
        input_->setFixedSize(60, 20);
        input_->setPlaceholderText("cm");
        input_->setStyleSheet("background:#001a1a; color:#44ffff; border:1px solid #44ffff;");
        auto* proxy = new QGraphicsProxyWidget(this);
        proxy->setWidget(input_);
        proxy->setPos(34, 18);
        connect(input_, &QLineEdit::textChanged, this, [this](const QString& text) {
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

    // Called by CanvasWidget after inputChanged is connected -- setting this
    // in the constructor would fire textChanged before anything outside this
    // object could possibly be connected yet, silently dropping the signal.
    void emitInitialValue() override {
        input_->setText("10");
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "DistanceSensor",
        {"TRIG", "ECHO", "DISTANCE", "ULTRASONIC", "SONAR", "HCSR"},
        {
            {"TRIG", {"TRIG"}},
            {"ECHO", {"ECHO"}},
        },
        {"pulseIn("},
        false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new DistanceSensorItem(pin, parent);
        },
        MultiPinStrategy::Suffix,
        "ECHO"
    });
    return true;
}();