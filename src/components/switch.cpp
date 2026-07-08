#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor SWITCH_ACTIVE  ("#49caf9");
static const QColor SWITCH_INACTIVE("#02455e");

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
        QColor fill = switch_ ? SWITCH_ACTIVE : SWITCH_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "Switch");
    }

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        switch_ = !switch_;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, switch_ ? 1 : 0);
    }
};

static bool reg_switch = []() {
    ComponentRegistry::instance().register_component({
        "Switch",
        {"TOGGLE", "SWITCH"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new SwitchItem(pin, parent);
        }
    });
    return true;
}();