#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QRadialGradient>
#include <QCursor>

static const QColor BUTTON_ACTIVE("#dc74c2");

// Tactile pushbutton body: a dark plastic base plate with 4 corner legs and
// a round cap that shrinks and tints on press. Shared by ButtonItem and
// ButtonCleanItem below -- same physical shape, different pin semantics.
static void paintButtonCap(QPainter* p, const QRectF& r, bool pressed) {
    QRectF base = r.adjusted(r.width() * 0.15, r.height() * 0.15, -r.width() * 0.15, -r.height() * 0.15);
    QColor plate("#2b2b2b");
    QLinearGradient bg(base.topLeft(), base.bottomLeft());
    bg.setColorAt(0.0, plate.lighter(130));
    bg.setColorAt(0.5, plate);
    bg.setColorAt(1.0, plate.darker(120));
    p->setPen(QPen(plate.darker(180), 1.2));
    p->setBrush(bg);
    p->drawRoundedRect(base, 4, 4);

    p->setPen(Qt::NoPen);
    p->setBrush(QColor("#999"));
    qreal legW = 4, legH = 6;
    for (const auto& pt : {base.topLeft(), base.topRight(), base.bottomLeft(), base.bottomRight()})
        p->drawRect(QRectF(pt.x() - legW / 2, pt.y() - legH / 2, legW, legH));

    QPointF c = base.center();
    qreal capR = qMin(base.width(), base.height()) * (pressed ? 0.28 : 0.34);
    QColor capColor = pressed ? BUTTON_ACTIVE : QColor("#e0e0e0");
    QRadialGradient cap(c - QPointF(capR * 0.3, capR * 0.3), capR * 1.6);
    cap.setColorAt(0.0, capColor.lighter(140));
    cap.setColorAt(1.0, capColor.darker(120));
    p->setPen(QPen(capColor.darker(160), 1));
    p->setBrush(cap);
    p->drawEllipse(c, capR, capR);

    p->setPen(QPen(capColor.darker(200), 1.4));
    p->drawLine(c - QPointF(capR * 0.5, 0), c + QPointF(capR * 0.5, 0));
    p->drawLine(c - QPointF(0, capR * 0.5), c + QPointF(0, capR * 0.5));

    // Straight lead on the right edge -- buttons are inputs, so
    // CanvasWidget::updateWires attaches the wire at local (width, 15).
    p->setPen(QPen(QColor("#999"), 2));
    p->drawLine(QPointF(r.width() - 10, 15), QPointF(r.width(), 15));
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
        paintButtonCap(p, boundingRect(), pressed_);
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
        paintButtonCap(p, boundingRect(), pressed_);
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