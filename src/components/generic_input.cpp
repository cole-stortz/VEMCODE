#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLinearGradient>
#include <QCursor>

static const QColor GENERIC_INPUT_ACTIVE  ("#748edc");
static const QColor GENERIC_INPUT_INACTIVE("#101a37");

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
        QColor fill = active_ ? GENERIC_INPUT_ACTIVE : GENERIC_INPUT_INACTIVE;
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
        p->drawLine(c - QPointF(6, 0), c + QPointF(10, 0));
        p->drawLine(c - QPointF(6, 0), c - QPointF(1, -5));
        p->drawLine(c - QPointF(6, 0), c - QPointF(1, 5));

        p->setFont(QFont("Courier New", 7));
        p->drawText(r, Qt::AlignHCenter | Qt::AlignTop, "IN");

        // Straight lead on the right edge -- generic inputs are inputs, so
        // CanvasWidget::updateWires attaches the wire at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 10, 15), QPointF(r.width(), 15));
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

// Never matched through the registry's keyword/pattern tiers -- CircuitDetector
// assigns this type directly as its final fallback when nothing else matches
// (see infer_type()), so detect_single/detect_multi/detect_pattern stay empty.
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