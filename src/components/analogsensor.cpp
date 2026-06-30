#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>

class AnalogSensorItem : public ComponentItem {
public:
    AnalogSensorItem(int p, QGraphicsItem* parent)
        : ComponentItem(p, parent) {
        auto* input = new QLineEdit("0");
        input->setFixedSize(60, 20);
        input->setPlaceholderText("0-1023");
        input->setStyleSheet("background:#1a1a00; color:#ffff44; border:1px solid #ffff44;");
        auto* proxy = new QGraphicsProxyWidget(this);
        proxy->setWidget(input);
        proxy->setPos(34, 18);
        connect(input, &QLineEdit::textChanged, this, [this](const QString& text) {
            bool ok;
            int val = text.toInt(&ok);
            if (ok)
                emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, qBound(0, val, 1023));
        });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        static const QColor fill("#2a2a2a");
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 16), Qt::AlignLeft, "Sensor");
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "AnalogSensor",
        {"LIGHT", "LDR", "PHOTO", "TEMP", "TEMPERATURE", "NTC", "SENSOR"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new AnalogSensorItem(pin, parent);
        }
    });
    return true;
}();