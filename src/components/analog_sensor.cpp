#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>

static const QColor ANALOG_SENSOR_FILL("#2a2a2a");

class AnalogSensorItem : public ComponentItem {
    QLineEdit* input_;

public:
    AnalogSensorItem(int p, QGraphicsItem* parent)
        : ComponentItem(p, parent) {
        input_ = new QLineEdit();
        input_->setFixedSize(60, 20);
        input_->setPlaceholderText("0-1023");
        input_->setStyleSheet("background:#1a1a00; color:#ffff44; border:1px solid #ffff44;");
        auto* proxy = new QGraphicsProxyWidget(this);
        proxy->setWidget(input_);
        proxy->setPos(34, 18);
        connect(input_, &QLineEdit::textChanged, this, [this](const QString& text) {
            bool ok;
            int val = text.toInt(&ok);
            if (ok)
                emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, qBound(0, val, 1023));
        });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = ANALOG_SENSOR_FILL;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 16), Qt::AlignLeft, "Sensor");
    }

    // Called by CanvasWidget after inputChanged is connected -- see
    // DistanceSensorItem for why this can't just happen in the constructor.
    void emitInitialValue() override {
        input_->setText("0");
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "AnalogSensor",
        {"LIGHT", "LDR", "PHOTO", "TEMP", "TEMPERATURE", "THERMISTOR", "NTC", "SENSOR", "ANALOG", "ADC"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new AnalogSensorItem(pin, parent);
        }
    });
    return true;
}();