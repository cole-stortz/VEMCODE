#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor GENERIC_INPUT_ACTIVE  ("#6c5cd1");

class GenericInputItem : public ComponentItem {
    bool active_ = false;

public:
    GenericInputItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();

        // Straight lead on the right edge; wire attaches at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 30, 15), QPointF(r.width(), 15));

        QColor fill = active_ ? GENERIC_INPUT_ACTIVE : themedHousing(GENERIC_INPUT_ACTIVE, isDarkTheme(), 400);
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
        p->drawText(box.adjusted(0, 2, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "IN");
    }

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        active_ = !active_;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, active_ ? 1 : 0);
    }

    void emitInitialValue() override {
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, active_ ? 1 : 0);
    }
};

// Never matched through the registry's keyword/pattern tiers -- CircuitDetector assigns
// this directly as its final fallback (see infer_type()), so detect_* stays empty.
static bool registered = []() {
    ComponentDefinition def{
        "GenericInput",
        {}, {}, {},
        false, // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new GenericInputItem(pin, parent);
        }
    };
    def.wire_color = GENERIC_INPUT_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();