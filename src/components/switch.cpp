#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor SWITCH_ACTIVE("#3ac2d1");

class SwitchItem : public ComponentItem {
    bool switch_ = false;
public:
    SwitchItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();

        // Lead drawn under the body, extending past its edge -- the body
        // paints over the overlap, so the visible stub always ends up flush
        // with the body's edge regardless of the exact inset.
        // Switches are inputs, so CanvasWidget::updateWires attaches the
        // wire at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 10, 15), QPointF(r.width(), 15));

        QRectF body = r.adjusted(r.width() * 0.1, r.height() * 0.1, -r.width() * 0.1, -r.height() * 0.1);

        QColor housing(SWITCH_ACTIVE.darker(300));
        p->setPen(QPen(housing.darker(180), 3));
        p->setBrush(housing);
        p->drawRoundedRect(body, 4, 4);

        QRectF trackArea = body.adjusted(4, 4, -4, -4);
        qreal squareSize = trackArea.height();
        QColor leverColor = switch_ ? SWITCH_ACTIVE : QColor("#666");
        QRectF lever(switch_ ? trackArea.right() - squareSize : trackArea.left(),
                     trackArea.top(), squareSize, squareSize);
        p->setPen(QPen(leverColor.darker(180), 2));
        p->setBrush(leverColor);
        p->drawRoundedRect(lever, 2, 2);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        switch_ = !switch_;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, switch_ ? 1 : 0);
    }

    void emitInitialValue() override {
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, switch_ ? 1 : 0);
    }
};

static bool reg_switch = []() {
    ComponentDefinition def{
        "Switch",
        {"TOGGLE", "SWITCH"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new SwitchItem(pin, parent);
        }
    };
    def.wire_color = SWITCH_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();