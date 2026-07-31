#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor GENERIC_ACTIVE  ("#c23a9c");
static const QColor GENERIC_INACTIVE = GENERIC_ACTIVE.darker(400);

class GenericOutputItem : public ComponentItem {
    bool active_ = false;

public:
    GenericOutputItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();

        // Straight lead on the left edge -- generic outputs are outputs, so
        // CanvasWidget::updateWires attaches the wire at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(30, 15), QPointF(0, 15));

        QColor fill = active_ ? GENERIC_ACTIVE : GENERIC_INACTIVE;
        qreal margin = 4;
        qreal squareSize = r.height() - 2 * margin;
        QRectF box(r.center().x() - squareSize / 2, margin, squareSize, squareSize);

        p->setPen(QPen(fill.darker(180), 3));
        p->setBrush(fill);
        p->drawRoundedRect(box, 4, 4);

        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        QColor tc = lum > 128 ? QColor("#1a1a1a") : QColor("#e8e8e8");

        QPointF c = box.center();
        QPointF tip(c.x() + 10, c.y());
        QPointF baseCenter(c.x() + 4, c.y());
        QPolygonF arrowHead;
        arrowHead << tip << QPointF(baseCenter.x(), baseCenter.y() - 4) << QPointF(baseCenter.x(), baseCenter.y() + 4);
        p->setPen(Qt::NoPen);
        p->setBrush(tc);
        p->drawPolygon(arrowHead);

        p->setPen(QPen(tc, 2, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(c.x() - 6, c.y()), baseCenter);

        p->setFont(QFont("Courier New", 7));
        p->drawText(box.adjusted(0, 2, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "OUT");
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