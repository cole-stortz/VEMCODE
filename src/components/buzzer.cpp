#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QRadialGradient>

static const QColor BUZZER_ACTIVE("#dc9b74");

class BuzzerItem : public ComponentItem {
    bool active_;

public:
    BuzzerItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), active_(false) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        QPointF c = r.center();
        qreal rad = qMin(r.width(), r.height()) * 0.36;
        QColor base = active_ ? BUZZER_ACTIVE : QColor("#4a3624");

        QRadialGradient body(c, rad);
        body.setColorAt(0.0, base.lighter(130));
        body.setColorAt(0.75, base);
        body.setColorAt(1.0, base.darker(150));
        p->setPen(QPen(base.darker(180), 1.2));
        p->setBrush(body);
        p->drawEllipse(c, rad, rad);

        p->setPen(QPen(base.darker(200), 1));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(c, rad * 0.6, rad * 0.6);

        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#1a1a1a"));
        p->drawEllipse(c, rad * 0.18, rad * 0.18);

        // Straight lead on the left edge -- buzzers are outputs, so
        // CanvasWidget::updateWires attaches the wire at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(10, 15), QPointF(0, 15));

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