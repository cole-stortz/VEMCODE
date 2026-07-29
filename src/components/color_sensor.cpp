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
    QColor lastSensed_ = COLOR_SENSOR_FILL;

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

        r_in_ = make_input("#ff4444", "#1a0000", 7);
        g_in_ = make_input("#44ff44", "#001a00", 37);
        b_in_ = make_input("#4444ff", "#00001a", 67);

        connect(r_in_, &QLineEdit::textChanged, this, [this] { emitColor(); });
        connect(g_in_, &QLineEdit::textChanged, this, [this] { emitColor(); });
        connect(b_in_, &QLineEdit::textChanged, this, [this] { emitColor(); });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 64); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        p->setPen(QPen(COLOR_SENSOR_FILL.darker(500), 3));
        p->setBrush(QColor(COLOR_SENSOR_FILL.darker(300)));
        p->drawRoundedRect(r, 4, 4);

        QPointF c(r.center().x(), r.top() + r.height() * 0.32);
        qreal lensR = qMin(r.width(), r.height()) * 0.22;
        p->setPen(QPen(COLOR_SENSOR_FILL.darker(500), 1));
        p->setBrush(lastSensed_);
        p->drawEllipse(c, lensR, lensR);

        qreal off = qMin(r.width(), r.height()) * 0.3;
        for (const auto& d : {QPointF(-off, -off * 0.6), QPointF(off, -off * 0.6),
                               QPointF(-off, off * 0.6), QPointF(off, off * 0.6)}) {
            p->setPen(QPen(QColor("#3a3a3a").darker(200), 1));
            p->setBrush(QColor("#3a3a3a"));
            p->drawEllipse(c + d, 3, 3);
        }

        // Straight leads on the right edge, one per OUT/S2/S3 pin slot --
        // color sensors are inputs, so CanvasWidget::updateWires attaches
        // wire i at local (width, 15 + i*5), same spacing as WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 3; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(r.width() - 5, ly), QPointF(r.width(), ly));
        }
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
            lastSensed_ = QColor(r, g, b);
            QVariantList payload{ r, g, b, s2Pin_, s3Pin_ };
            emit inputChanged(pin(), (int)ComponentEventType::ColorRGB, payload);
            update();
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
        MultiPinStrategy::Singleton,
        "OUT"
    };
    def.wire_color = COLOR_SENSOR_FILL;
    ComponentRegistry::instance().register_component(def);

    // Separate entry for the array-of-pins style (e.g. `const int S2[] = {A2};`)
    // some ported sketches use instead of plain #defines -- same type_name/
    // create_item, same tradeoff HBridgeMotor's bare ENA/IN1/IN2 entry accepts.
    ComponentDefinition arrayDef = def;
    arrayDef.multi_pin_strategy = MultiPinStrategy::Array;
    ComponentRegistry::instance().register_component(arrayDef);

    return true;
}();