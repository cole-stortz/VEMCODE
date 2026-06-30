#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor BUTTON_ACTIVE  ("#44ff88");
static const QColor BUTTON_INACTIVE("#003a15");

class ButtonItem : public ComponentItem {
    bool pressed_ = false;
public:
    ButtonItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = pressed_ ? BUTTON_ACTIVE : BUTTON_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "Button");
    }

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        pressed_ = true;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::BouncePress, 0);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override {
        pressed_ = false;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::BouncePress, 1);
    }
};

class ButtonCleanItem : public ComponentItem {
    bool pressed_ = false;
public:
    ButtonCleanItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = pressed_ ? BUTTON_ACTIVE : BUTTON_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "Button (Clean)");
    }

    void mousePressEvent(QGraphicsSceneMouseEvent*) override {
        pressed_ = true;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, 0);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override {
        pressed_ = false;
        update();
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, 1);
    }
};

static bool reg_button = []() {
    ComponentRegistry::instance().register_component({
        "Button",
        {"BUTTON", "BTN", "TACT", "PUSH"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new ButtonItem(pin, parent);
        }
    });
    return true;
}();

static bool reg_button_clean = []() {
    ComponentRegistry::instance().register_component({
        "ButtonClean",
        {"CLEAN", "IDEAL"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new ButtonCleanItem(pin, parent);
        }
    });
    return true;
}();