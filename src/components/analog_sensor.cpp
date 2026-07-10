#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <qgraphicsitem.h>

static const QColor LIGHT_SENSOR_FILL("#dccb74");
static const QColor TEMPERATURE_SENSOR_FILL("#dc8674");
static const QColor FORCE_SENSOR_FILL("#5a5a5a");
static const QColor ANALOG_SENSOR_FILL("#8174dc");

class AnalogSensorItemBase : public ComponentItem {
    QLineEdit* input_;
    QColor fill_;
    QString label_;

public:
    AnalogSensorItemBase(int p, QGraphicsItem* parent, const QColor& fill, const QString& label)
        : ComponentItem(p, parent), fill_(fill), label_(label) {
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
        p->setPen(QPen(fill_.darker(150), 1));
        p->setBrush(fill_);
        p->drawRect(boundingRect());
        int lum = (fill_.red() * 299 + fill_.green() * 587 + fill_.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 16), Qt::AlignLeft, label_);
    }

    // Called by CanvasWidget after inputChanged is connected -- see
    // DistanceSensorItem for why this can't just happen in the constructor.
    void emitInitialValue() override {
        input_->setText("0");
    }
};

class LightSensorItem : public AnalogSensorItemBase {
public:
    LightSensorItem(int p, QGraphicsItem* parent)
        : AnalogSensorItemBase(p, parent, LIGHT_SENSOR_FILL, "Light") {}
};

class TemperatureSensorItem : public AnalogSensorItemBase {
public:
    TemperatureSensorItem(int p, QGraphicsItem* parent)
        : AnalogSensorItemBase(p, parent, TEMPERATURE_SENSOR_FILL, "Temp") {}
};

class ForceSensorItem : public AnalogSensorItemBase {
public:
    ForceSensorItem(int p, QGraphicsItem* parent)
        : AnalogSensorItemBase(p, parent, FORCE_SENSOR_FILL, "Force") {}
};

class AnalogSensorItem : public AnalogSensorItemBase {
public:
    AnalogSensorItem(int p, QGraphicsItem* parent)
        : AnalogSensorItemBase(p, parent, ANALOG_SENSOR_FILL, "Sensor") {}
};

// Registered before AnalogSensor so its more specific keywords win --
// registration order across files is unspecified, but within this file
// it's guaranteed top-to-bottom, and find_by_single_keyword also prefers
// the longest matching keyword regardless of order.
static bool reg_light_sensor = []() {
    ComponentDefinition def{
        "LightSensor",
        {"LIGHT", "LDR", "PHOTO"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new LightSensorItem(pin, parent);
        }
    };
    def.wire_color = LIGHT_SENSOR_FILL;
    ComponentRegistry::instance().register_component(def);
    return true;
}();

static bool reg_temperature_sensor = []() {
    ComponentDefinition def{
        "TemperatureSensor",
        {"TEMP", "TEMPERATURE", "THERMISTOR", "NTC"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new TemperatureSensorItem(pin, parent);
        }
    };
    def.wire_color = TEMPERATURE_SENSOR_FILL;
    ComponentRegistry::instance().register_component(def);
    return true;
}();

static bool reg_force_sensor = []() {
    ComponentDefinition def{
        "ForceSensor",
        {"FORCE", "FORCESENSOR", "FORCE_SENSOR"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new ForceSensorItem(pin, parent);
        }
    };
    def.wire_color = FORCE_SENSOR_FILL;
    ComponentRegistry::instance().register_component(def);
    return true;
}();

static bool registered = []() {
    ComponentDefinition def{
        "AnalogSensor",
        {"SENSOR", "ANALOG", "ADC"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new AnalogSensorItem(pin, parent);
        }
    };
    def.wire_color = ANALOG_SENSOR_FILL;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
