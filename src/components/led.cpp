#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QRadialGradient>

static const QColor LED_ACTIVE  ("#e0b82e");
static const QColor LED_INACTIVE("#323710");

class LedItem : public ComponentItem {
    bool active_;
    QColor baseColor_ = LED_ACTIVE; // configurable -- the "on" color

    // Derives a dimmed "off" color from the base color, same proportions as the original
    // hardcoded LED_ACTIVE/LED_INACTIVE pair.
    static QColor dimmed(const QColor& c, bool dark) {
        return themedHousing(c, dark, 400);
    }

public:
    LedItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), active_(false) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();

        // Lead drawn under the bulb, extending past its edge, so the stub stays flush
        // regardless of radius; wire attaches at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(35, 15), QPointF(0, 15));

        QPointF c = r.center();
        qreal rad = qMin(r.width(), r.height()) * 0.40;

        if (active_) {
            QRadialGradient glow(c, rad * 1.8);
            QColor g1 = baseColor_; g1.setAlpha(140);
            QColor g2 = baseColor_; g2.setAlpha(0);
            glow.setColorAt(0.0, g1);
            glow.setColorAt(1.0, g2);
            p->setPen(Qt::NoPen);
            p->setBrush(glow);
            p->drawEllipse(c, rad * 2.6, rad * 2.6);
        }

        QColor body = active_ ? baseColor_ : dimmed(baseColor_, isDarkTheme());
        p->setPen(QPen(baseColor_.darker(200), 3));
        p->setBrush(body);
        p->drawEllipse(c, rad, rad);
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
        {},    // detect_multi -- none
        {},    // detect_pattern -- none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new LedItem(pin, parent);
        }
    };
    def.wire_color = LED_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();