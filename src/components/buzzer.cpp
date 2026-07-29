#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor BUZZER_ACTIVE("#dc9b74");

class BuzzerItem : public ComponentItem {
    bool active_;

public:
    BuzzerItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), active_(false) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();

        // Lead drawn under the body, extending past its left edge -- the
        // body paints over the overlap, so the visible stub always ends up
        // flush with the body's edge without having to keep the two in sync.
        // CanvasWidget::updateWires attaches the wire at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(40, 15), QPointF(0, 15));

        QPointF c = r.center();
        qreal rad = qMin(r.width(), r.height()) * 0.45;
        QColor base = active_ ? BUZZER_ACTIVE : QColor("#4a3624");

        p->setPen(QPen(base.darker(180), 3));
        p->setBrush(base);
        p->drawEllipse(c, rad, rad);

        p->setPen(QPen(base.darker(200), 1));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(c, rad * 0.6, rad * 0.6);

        p->setPen(Qt::NoPen);
        p->setBrush(base.darker(300));
        p->drawEllipse(c, rad * 0.18, rad * 0.18);

        if (active_) {
            p->setPen(QPen(BUZZER_ACTIVE, 1.4));
            for (int i = 1; i <= 3; ++i) {
                qreal a = rad + i * 5;
                QRectF arcRect(c.x() + rad * 0.7, c.y() - a / 2, a * 0.6, a);
                p->drawArc(arcRect, -60 * 16, 120 * 16);
            }
        }
    }

    void onPinChanged(int, int value) override {
        active_ = (value > 0);
        update();
    }
};

static bool registered = []() {
    ComponentDefinition def{
        "Buzzer",
        {"BUZZER", "BUZZ", "SPEAKER", "TONE", "PIEZO"},
        {},    // detect_multi — none
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new BuzzerItem(pin, parent);
        }
    };
    def.wire_color = BUZZER_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();