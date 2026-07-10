#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>

static const QColor COLOR_SENSOR_FILL("#a874dc");

class ColorSensorItem : public ComponentItem {
    int s2Pin_ = -1;
    int s3Pin_ = -1;
    QLineEdit* r_in_;
    QLineEdit* g_in_;
    QLineEdit* b_in_;

public:
    ColorSensorItem(int p, QGraphicsItem* parent)
        : ComponentItem(p, parent) {
        auto make_input = [this](const char* fg, const char* bg, int x) -> QLineEdit* {
            QLineEdit* in = new QLineEdit("0");
            in->setFixedSize(26, 16);
            in->setStyleSheet(QString("background:%1; color:%2; border:1px solid %2;")
                              .arg(bg).arg(fg));
            auto* proxy = new QGraphicsProxyWidget(this);
            proxy->setWidget(in);
            proxy->setPos(x, 42);
            return in;
        };

        r_in_ = make_input("#ff4444", "#1a0000", 4);
        g_in_ = make_input("#44ff44", "#001a00", 34);
        b_in_ = make_input("#4444ff", "#00001a", 64);

        connect(r_in_, &QLineEdit::textChanged, this, [this] { emitColor(); });
        connect(g_in_, &QLineEdit::textChanged, this, [this] { emitColor(); });
        connect(b_in_, &QLineEdit::textChanged, this, [this] { emitColor(); });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 64); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = COLOR_SENSOR_FILL;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 16), Qt::AlignLeft, "Color");
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) s2Pin_ = pins[1];
        if (pins.size() > 2) s3Pin_ = pins[2];
    }

    // Called by CanvasWidget after inputChanged is connected and
    // configureMultiPin has run, so s2Pin_/s3Pin_ are already valid.
    void emitInitialValue() override {
        emitColor();
    }

private:
    void emitColor() {
        bool rok, gok, bok;
        int r = qBound(0, r_in_->text().toInt(&rok), 255);
        int g = qBound(0, g_in_->text().toInt(&gok), 255);
        int b = qBound(0, b_in_->text().toInt(&bok), 255);
        if (rok && gok && bok) {
            QVariantList payload{ r, g, b, s2Pin_, s3Pin_ };
            emit inputChanged(pin(), (int)ComponentEventType::ColorRGB, payload);
        }
    }
};

static bool registered = []() {
    ComponentDefinition def{
        "ColorSensor",
        {"COLOR", "TCS", "S2", "S3"},
        {
            {"OUT", {"OUT", "SENSOROUT", "OUTPUT"}},
            {"S2",  {"S2"}},
            {"S3",  {"S3"}},
        },
        {"pulseIn("},
        false, // is_output — this is an input component
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new ColorSensorItem(pin, parent);
        },
        MultiPinStrategy::Array,
        "OUT"
    };
    def.wire_color = COLOR_SENSOR_FILL;
    ComponentRegistry::instance().register_component(def);
    return true;
}();