#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QRadialGradient>

static const QColor LED_ACTIVE  ("#cfdc74");
static const QColor LED_INACTIVE("#323710");

class LedItem : public ComponentItem {
    bool active_;
    QColor baseColor_ = LED_ACTIVE; // configurable -- the "on" color

    // Derives a dimmed "off" color from the current base color, same
    // proportions as the original hardcoded LED_ACTIVE/LED_INACTIVE pair.
    static QColor dimmed(const QColor& c) {
        return c.darker(400);
    }

public:
    LedItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), active_(false) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        QPointF c = r.center();
        qreal rad = qMin(r.width(), r.height()) * 0.30;

        if (active_) {
            QRadialGradient glow(c, rad * 2.6);
            QColor g1 = baseColor_; g1.setAlpha(140);
            QColor g2 = baseColor_; g2.setAlpha(0);
            glow.setColorAt(0.0, g1);
            glow.setColorAt(1.0, g2);
            p->setPen(Qt::NoPen);
            p->setBrush(glow);
            p->drawEllipse(c, rad * 2.6, rad * 2.6);
        }

        QColor body = active_ ? baseColor_ : dimmed(baseColor_);
        QRadialGradient bulb(c - QPointF(rad * 0.3, rad * 0.3), rad * 1.6);
        bulb.setColorAt(0.0, body.lighter(active_ ? 170 : 140));
        bulb.setColorAt(0.6, body);
        bulb.setColorAt(1.0, body.darker(140));
        p->setPen(QPen(baseColor_.darker(200), 1.2));
        p->setBrush(bulb);
        p->drawEllipse(c, rad, rad);

        // Straight lead on the left edge -- LEDs are outputs, so
        // CanvasWidget::updateWires attaches the wire at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(10, 15), QPointF(0, 15));
    }

    void onPinChanged(int, int value) override {
        active_ = (value > 0);
        update();
    }

    bool supportsColorConfig() const override { return true; }
    QColor baseColor() const override { return baseColor_; }
    void configureColor(const QColor& color) override {
        baseColor_ = color;
        update();
    }
};

static bool registered = []() {
    ComponentDefinition def{
        "LED",
        {"LED", "LAMP", "DIODE", "INDICATOR"},
        {},    // detect_multi — none
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new LedItem(pin, parent);
        }
    };
    def.wire_color = LED_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();