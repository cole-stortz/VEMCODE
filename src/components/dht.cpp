#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLinearGradient>
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
        QRectF r = boundingRect();
        QColor base = DHT_FILL;
        QLinearGradient bg(r.topLeft(), r.bottomLeft());
        bg.setColorAt(0.0, base.lighter(130));
        bg.setColorAt(0.5, base);
        bg.setColorAt(1.0, base.darker(120));
        p->setPen(QPen(base.darker(180), 1.2));
        p->setBrush(bg);
        p->drawRoundedRect(r, 5, 5);

        // Grille fills the top band only -- the bottom third is left clear for
        // the temp_in_/humid_in_ QLineEdit proxies, which double as the
        // human-readable readout (no point painting a duplicate screen
        // underneath widgets that would just cover it).
        QRectF grille = r.adjusted(6, 6, -6, -r.height() * 0.42);
        p->setPen(QPen(base.darker(160), 1));
        p->setBrush(Qt::NoBrush);
        int cols = 6, rows = 3;
        qreal cw = grille.width() / cols, ch = grille.height() / rows;
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col)
                p->drawRect(QRectF(grille.x() + col * cw + 1, grille.y() + row * ch + 1, cw - 2, ch - 2));

        // Straight lead on the right edge -- DHT is an input, so
        // CanvasWidget::updateWires attaches the wire at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 10, 15), QPointF(r.width(), 15));
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
