#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLinearGradient>

static const QColor GENERIC_ACTIVE  ("#dc749b");
static const QColor GENERIC_INACTIVE("#37101f");

class GenericOutputItem : public ComponentItem {
    bool active_ = false;

public:
    GenericOutputItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = active_ ? GENERIC_ACTIVE : GENERIC_INACTIVE;
        QRectF r = boundingRect();
        QLinearGradient bg(r.topLeft(), r.bottomLeft());
        bg.setColorAt(0.0, fill.lighter(130));
        bg.setColorAt(0.5, fill);
        bg.setColorAt(1.0, fill.darker(120));
        p->setPen(QPen(fill.darker(180), 1.2));
        p->setBrush(bg);
        p->drawRoundedRect(r, 4, 4);

        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        QColor tc = lum > 128 ? QColor("#1a1a1a") : QColor("#e8e8e8");

        QPointF c = r.center();
        p->setPen(QPen(tc, 2));
        p->drawLine(c - QPointF(10, 0), c + QPointF(6, 0));
        p->drawLine(c + QPointF(6, 0), c + QPointF(1, -5));
        p->drawLine(c + QPointF(6, 0), c + QPointF(1, 5));

        p->setFont(QFont("Courier New", 7));
        p->drawText(r, Qt::AlignHCenter | Qt::AlignTop, "OUT");

        // Straight lead on the left edge -- generic outputs are outputs, so
        // CanvasWidget::updateWires attaches the wire at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(10, 15), QPointF(0, 15));
    }

    void onPinChanged(int, int value) override {
        active_ = (value > 0);
        update();
    }
};

// Never matched through the registry's keyword/pattern tiers -- CircuitDetector
// assigns this type directly as its final fallback when nothing else matches
// (see infer_type()), so detect_single/detect_multi/detect_pattern stay empty.
static bool registered = []() {
    ComponentDefinition def{
        "GenericOutput",
        {}, {}, {},
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new GenericOutputItem(pin, parent);
        }
    };
    def.wire_color = GENERIC_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();