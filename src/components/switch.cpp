#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLinearGradient>
#include <QCursor>

static const QColor SWITCH_ACTIVE("#74dcdc");

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
        QRectF body = r.adjusted(r.width() * 0.2, r.height() * 0.15, -r.width() * 0.2, -r.height() * 0.15);

        QColor housing("#333");
        QLinearGradient bg(body.topLeft(), body.bottomLeft());
        bg.setColorAt(0.0, housing.lighter(130));
        bg.setColorAt(0.5, housing);
        bg.setColorAt(1.0, housing.darker(120));
        p->setPen(QPen(housing.darker(180), 1.2));
        p->setBrush(bg);
        p->drawRoundedRect(body, body.width() * 0.3, body.width() * 0.3);

        QRectF leverArea = body.adjusted(body.width() * 0.15, body.height() * 0.12,
                                          -body.width() * 0.15, -body.height() * 0.12);
        QColor leverColor = switch_ ? SWITCH_ACTIVE : QColor("#666");
        QRectF lever(leverArea.x(), switch_ ? leverArea.y() : leverArea.center().y(),
                     leverArea.width(), leverArea.height() / 2);
        QLinearGradient lg(lever.topLeft(), lever.bottomLeft());
        lg.setColorAt(0.0, leverColor.lighter(130));
        lg.setColorAt(0.5, leverColor);
        lg.setColorAt(1.0, leverColor.darker(120));
        p->setPen(QPen(leverColor.darker(180), 1.2));
        p->setBrush(lg);
        p->drawRoundedRect(lever, lever.width() * 0.3, lever.width() * 0.3);

        // Straight lead on the right edge -- switches are inputs, so
        // CanvasWidget::updateWires attaches the wire at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 10, 15), QPointF(r.width(), 15));
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