#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>

static const QColor BUTTON_ACTIVE("#e8639c");

// Tactile pushbutton body: a flat dark plastic base plate with a round cap
// that shrinks and tints on press. Shared by ButtonItem and ButtonCleanItem
// below -- same physical shape, different pin semantics.
static void paintButtonCap(QPainter* p, const QRectF& r, bool pressed, bool dark) {
    // Lead drawn under the plate, extending past its right edge -- the
    // plate paints over the overlap, so the visible stub always ends up
    // flush with the plate's edge without having to keep the two in sync.
    // CanvasWidget::updateWires attaches the wire at local (width, 15).
    p->setPen(QPen(QColor("#999"), 2));
    p->drawLine(QPointF(r.width() - 25, 15), QPointF(r.width(), 15));

    QRectF base = r.adjusted(r.width() * 0.2, r.height() * 0.05, -r.width() * 0.2, -r.height() * 0.05);
    QColor plate = themedHousing(BUTTON_ACTIVE, dark);
    p->setPen(QPen(plate.darker(180), 3));
    p->setBrush(plate);
    p->drawRoundedRect(base, 4, 4);

    QPointF c = base.center();
    qreal capR = qMin(base.width(), base.height()) * (pressed ? 0.35 : 0.40);
    QColor capColor = pressed ? BUTTON_ACTIVE : (dark ? QColor("#a6a6a6") : QColor("#787878"));
    p->setPen(QPen(capColor.darker(160), 1));
    p->setBrush(capColor);
    p->drawEllipse(c, capR, capR);
}

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
        paintButtonCap(p, boundingRect(), pressed_, isDarkTheme());
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

    void emitInitialValue() override {
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
        paintButtonCap(p, boundingRect(), pressed_, isDarkTheme());
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

    void emitInitialValue() override {
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, 1);
    }
};

// Registered before Button so ambiguous names (e.g. "CLEAN_BUTTON") match
// ButtonClean first -- registration order across files is unspecified, but
// within this file it's guaranteed top-to-bottom.
static bool reg_button_clean = []() {
    ComponentDefinition def{
        "ButtonClean",
        {"CLEAN", "IDEAL"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new ButtonCleanItem(pin, parent);
        }
    };
    def.wire_color = BUTTON_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();

static bool reg_button = []() {
    ComponentDefinition def{
        "Button",
        {"BUTTON", "BTN", "TACT", "PUSH", "KEY"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new ButtonItem(pin, parent);
        }
    };
    def.wire_color = BUTTON_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();