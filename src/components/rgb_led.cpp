#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QRadialGradient>


class RGBLedItem : public ComponentItem {
    int redpin_;
    int greenpin_ = -1;
    int bluepin_ = -1;

    int redval_ = 0;
    int greenval_ = 0;
    int blueval_ = 0;

    bool commonAnode_ = false;

    int applyPolarity(int value) const {
        return commonAnode_ ? (255 - value) : value;
    }

public:
    RGBLedItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), redpin_(pin) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill(redval_, greenval_, blueval_);
        bool lit = redval_ > 0 || greenval_ > 0 || blueval_ > 0;

        QRectF r = boundingRect();
        QPointF c = r.center();
        qreal rad = qMin(r.width(), r.height()) * 0.30;

        if (lit) {
            QRadialGradient glow(c, rad * 2.6);
            QColor g1 = fill; g1.setAlpha(140);
            QColor g2 = fill; g2.setAlpha(0);
            glow.setColorAt(0.0, g1);
            glow.setColorAt(1.0, g2);
            p->setPen(Qt::NoPen);
            p->setBrush(glow);
            p->drawEllipse(c, rad * 2.6, rad * 2.6);
        }

        QColor body = lit ? fill : QColor("#2a2a2a");
        QRadialGradient bulb(c - QPointF(rad * 0.3, rad * 0.3), rad * 1.6);
        bulb.setColorAt(0.0, body.lighter(lit ? 170 : 140));
        bulb.setColorAt(0.6, body);
        bulb.setColorAt(1.0, body.darker(140));
        p->setPen(QPen(body.darker(200), 1.2));
        p->setBrush(bulb);
        p->drawEllipse(c, rad, rad);

        // Straight leads on the left edge, one per R/G/B pin slot -- RGB LEDs
        // are outputs, so CanvasWidget::updateWires attaches wire i at local
        // (0, 15 + i*5), same spacing as WIRE_SPACING in canvaswidget.h.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 3; ++i) {
            qreal y = 15 + i * 5;
            p->drawLine(QPointF(10, y), QPointF(0, y));
        }
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) greenpin_ = pins[1];
        if (pins.size() > 2) bluepin_ = pins[2];
    }

    void onPinChanged(int pin, int value) override {
        int v = applyPolarity(qBound(0, value, 255));
        if (pin == redpin_)        redval_   = v;
        else if (pin == greenpin_) greenval_ = v;
        else if (pin == bluepin_)  blueval_  = v;
        update();
    }

    bool supportsPolarity() const override { return true; }
    bool isCommonAnode() const override { return commonAnode_; }
    void configurePolarity(bool commonAnode) override {
        commonAnode_ = commonAnode;
        update();
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "RGBLED",
        {},
        {
            {"RED",   {"REDPIN", "RPIN", "RED_PIN", "R_PIN"}},
            {"GREEN", {"GREENPIN", "GPIN","GREEN_PIN", "G_PIN"}},
            {"BLUE",  {"BLUEPIN", "BPIN", "BLUE_PIN","B_PIN"}},
        },
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new RGBLedItem(pin, parent);
        },
        MultiPinStrategy::Suffix,
        "RED"
    });
    return true;
}();
