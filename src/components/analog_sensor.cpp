#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <qgraphicsitem.h>
#include <QtMath>
#include <cmath>

static const QColor LIGHT_SENSOR_FILL("#d1c23a");
static const QColor TEMPERATURE_SENSOR_FILL("#b8522e");
static const QColor FORCE_SENSOR_FILL("#5c7ca8");
static const QColor ANALOG_SENSOR_FILL("#8e7ce8");

// Which pictogram to draw on the sensor's PCB face -- one entry per
// AnalogSensorItemBase subclass below.
enum class SensorIcon { Light, Temp, Force, Chip };

class AnalogSensorItemBase : public ComponentItem {
    QLineEdit* input_;
    QColor fill_;
    QString label_;
    SensorIcon icon_;

public:
    AnalogSensorItemBase(int p, QGraphicsItem* parent, const QColor& fill, const QString& label, SensorIcon icon)
        : ComponentItem(p, parent), fill_(fill), label_(label), icon_(icon) {
        input_ = new QLineEdit();
        input_->setFixedSize(60, 20);
        input_->setPlaceholderText("0-1023");
        input_->setStyleSheet("background:#1a1a00; color:#ffff44; border:1px solid #ffff44;");
        auto* proxy = new QGraphicsProxyWidget(this);
        proxy->setWidget(input_);
        proxy->setPos(34, 40);
        connect(input_, &QLineEdit::textChanged, this, [this](const QString& text) {
            bool ok;
            int val = text.toInt(&ok);
            if (ok)
                emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, qBound(0, val, 1023));
        });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 64); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        p->setPen(QPen(fill_.darker(180), 3));
        p->setBrush(fill_.darker(400));
        p->drawRoundedRect(r, 4, 4);

        // Icon sits in the top band, above the QLineEdit proxy that anchors
        // at y=40 -- the label caption goes above the icon, not the bottom,
        // since the bottom is reserved for that widget.
        p->setPen(fill_);
        p->setFont(QFont("Courier New", 7));
        p->drawText(QRectF(r.left(), r.top() + 2, r.width(), 10), Qt::AlignHCenter | Qt::AlignTop, label_);

        QPointF c(r.center().x(), r.top() + 24);
        qreal s = 7.0;
        p->setPen(QPen(fill_, 1.6));
        p->setBrush(Qt::NoBrush);

        switch (icon_) {
            case SensorIcon::Light:
                p->setBrush(fill_);
                p->drawEllipse(c, s * 0.5, s * 0.5);
                for (int i = 0; i < 8; ++i) {
                    qreal a = qDegreesToRadians(360.0 * i / 8);
                    QPointF p1(c.x() + std::cos(a) * s * 0.7, c.y() + std::sin(a) * s * 0.7);
                    QPointF p2(c.x() + std::cos(a) * s * 1.1, c.y() + std::sin(a) * s * 1.1);
                    p->drawLine(p1, p2);
                }
                break;
            case SensorIcon::Temp: {
                QRectF stem(c.x() - s * 0.15, c.y() - s, s * 0.3, s * 1.3);
                p->drawRoundedRect(stem, s * 0.15, s * 0.15);
                p->setBrush(fill_);
                p->drawEllipse(QPointF(c.x(), c.y() + s * 0.5), s * 0.4, s * 0.4);
                break;
            }
            case SensorIcon::Force: {
                QRectF arcRect(c.x() - s, c.y() - s * 0.6, s * 2, s * 1.6);
                p->drawArc(arcRect, 20 * 16, 140 * 16);
                p->setPen(QPen(fill_, 2));
                p->drawLine(c, c + QPointF(s * 0.5, -s * 0.4));
                break;
            }
            case SensorIcon::Chip: {
                QRectF chip(c.x() - s * 0.7, c.y() - s * 0.5, s * 1.4, s);
                p->drawRect(chip);
                for (int i = 0; i < 3; ++i) {
                    qreal x = chip.left() + chip.width() * (i + 1) / 4.0;
                    p->drawLine(QPointF(x, chip.top()), QPointF(x, chip.top() - 4));
                    p->drawLine(QPointF(x, chip.bottom()), QPointF(x, chip.bottom() + 4));
                }
                break;
            }
        }

        // Straight lead on the right edge -- these sensors are inputs, so
        // CanvasWidget::updateWires attaches the wire at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 10, 15), QPointF(r.width(), 15));
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
        : AnalogSensorItemBase(p, parent, LIGHT_SENSOR_FILL, "Light", SensorIcon::Light) {}
};

class TemperatureSensorItem : public AnalogSensorItemBase {
public:
    TemperatureSensorItem(int p, QGraphicsItem* parent)
        : AnalogSensorItemBase(p, parent, TEMPERATURE_SENSOR_FILL, "Temp", SensorIcon::Temp) {}
};

class ForceSensorItem : public AnalogSensorItemBase {
public:
    ForceSensorItem(int p, QGraphicsItem* parent)
        : AnalogSensorItemBase(p, parent, FORCE_SENSOR_FILL, "Force", SensorIcon::Force) {}
};

class AnalogSensorItem : public AnalogSensorItemBase {
public:
    AnalogSensorItem(int p, QGraphicsItem* parent)
        : AnalogSensorItemBase(p, parent, ANALOG_SENSOR_FILL, "Sensor", SensorIcon::Chip) {}
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
