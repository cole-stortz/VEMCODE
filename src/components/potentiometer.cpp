#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>

static const QColor POT_ACTIVE  ("#a8dc74");
static const QColor POT_INACTIVE("#243710");

class PotItem : public ComponentItem {
    bool dragging_ = false;
    int value_ = 512;
    int dragStartY_ = 0;
    int dragStartValue_ = 512;

public:
    PotItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::SizeVerCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        float ratio = value_ / 1023.0f;
        int r = POT_INACTIVE.red()   + ratio * (POT_ACTIVE.red()   - POT_INACTIVE.red());
        int g = POT_INACTIVE.green() + ratio * (POT_ACTIVE.green() - POT_INACTIVE.green());
        int b = POT_INACTIVE.blue()  + ratio * (POT_ACTIVE.blue()  - POT_INACTIVE.blue());
        QColor fill(r, g, b);
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, QString("Pot: %1").arg(value_));
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        dragging_ = true;
        dragStartY_ = event->pos().y();
        dragStartValue_ = value_;
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (!dragging_) return;
        int delta = dragStartY_ - event->pos().y();
        value_ = qBound(0, dragStartValue_ + delta * 4, 1023);
        update();
        emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, value_);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override {
        dragging_ = false;
    }
};

static bool reg_pot = []() {
    ComponentDefinition def{
        "Potentiometer",
        {"POT", "POTENTIOMETER", "KNOB", "DIAL"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new PotItem(pin, parent);
        }
    };
    def.wire_color = POT_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();