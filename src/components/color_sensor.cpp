#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>

class ColorSensorItem : public ComponentItem {
    int s2Pin_ = -1;
    int s3Pin_ = -1;

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

        QLineEdit* r_in = make_input("#ff4444", "#1a0000", 4);
        QLineEdit* g_in = make_input("#44ff44", "#001a00", 34);
        QLineEdit* b_in = make_input("#4444ff", "#00001a", 64);

        auto emit_color = [this, r_in, g_in, b_in]() {
            bool rok, gok, bok;
            int r = qBound(0, r_in->text().toInt(&rok), 255);
            int g = qBound(0, g_in->text().toInt(&gok), 255);
            int b = qBound(0, b_in->text().toInt(&bok), 255);
            if (rok && gok && bok) {
                QVariantList payload{ r, g, b, s2Pin_, s3Pin_ };
                emit inputChanged(pin(), (int)ComponentEventType::ColorRGB, payload);
            }
        };

        connect(r_in, &QLineEdit::textChanged, this, emit_color);
        connect(g_in, &QLineEdit::textChanged, this, emit_color);
        connect(b_in, &QLineEdit::textChanged, this, emit_color);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 64); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        static const QColor fill("#2a2a2a");
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 16), Qt::AlignLeft, "Color");
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) s2Pin_ = pins[1];
        if (pins.size() > 2) s3Pin_ = pins[2];
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
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
        }
    });
    return true;
}();