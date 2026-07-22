#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>

static const QColor DHT_FILL("#74a8dc");

class DhtItem : public ComponentItem {
    QLineEdit* temp_in_;
    QLineEdit* humid_in_;

public:
    DhtItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        auto make_input = [this](const char* text, int x) -> QLineEdit* {
            QLineEdit* in = new QLineEdit(text);
            in->setFixedSize(44, 16);
            in->setStyleSheet("background:#0a1420; color:#74a8dc; border:1px solid #74a8dc;");
            auto* proxy = new QGraphicsProxyWidget(this);
            proxy->setWidget(in);
            proxy->setPos(x, 42);
            return in;
        };

        temp_in_  = make_input("22.0", 4);
        humid_in_ = make_input("50.0", 52);

        connect(temp_in_,  &QLineEdit::textChanged, this, [this] { emitReading(); });
        connect(humid_in_, &QLineEdit::textChanged, this, [this] { emitReading(); });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 64); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = DHT_FILL;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 16), Qt::AlignLeft, "DHT");
        p->drawText(QRectF(4, 24, 44, 16), Qt::AlignLeft, "°C");
        p->drawText(QRectF(52, 24, 44, 16), Qt::AlignLeft, "%RH");
    }

    void emitInitialValue() override { emitReading(); }

private:
    void emitReading() {
        bool tok, hok;
        double t = temp_in_->text().toDouble(&tok);
        double h = qBound(0.0, humid_in_->text().toDouble(&hok), 100.0);
        if (tok && hok)
            emit inputChanged(pin(), (int)ComponentEventType::DhtReading, QVariantList{t, h});
    }
};

static bool reg_dht = []() {
    ComponentDefinition def{
        "DHT",
        {"DHT", "DHTPIN", "DHT_PIN"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new DhtItem(pin, parent);
        }
    };
    def.wire_color = DHT_FILL;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
