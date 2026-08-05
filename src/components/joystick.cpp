#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>

static const QColor JOY_ACTIVE("#6cb83a");

class JoystickItem : public ComponentItem {
    bool dragging_ = false;
    bool pressed_  = false;
    QPointF dragStartPos_;
    int vrx_ = 512;
    int vry_ = 512;
    int vryPin_ = -1;
    int swPin_  = -1;

public:
    JoystickItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
        setCursor(Qt::SizeAllCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) vryPin_ = pins[1];
        if (pins.size() > 2) swPin_  = pins[2];
    }

    void emitInitialValue() override {
        emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, vrx_);
        if (vryPin_ >= 0)
            emit inputChanged(vryPin_, (int)ComponentEventType::AnalogValue, vry_);
        if (swPin_ >= 0)
            emit inputChanged(swPin_, (int)ComponentEventType::DigitalPress, 1);
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        qreal dx = (vrx_ - 512) / 512.0;
        qreal dy = (vry_ - 512) / 512.0;

        QRectF r = boundingRect();
        QRectF body = r.adjusted(2, 2, -2, -2);
        QColor housing = themedHousing(JOY_ACTIVE, isDarkTheme(), 400);
        p->setPen(QPen(housing.darker(180), 3));
        p->setBrush(housing);
        p->drawRoundedRect(body, 6, 6);

        QPointF c = body.center();
        qreal travel = qMin(body.width(), body.height()) * 0.28;
        p->setPen(QPen(QColor("#000"), 1));
        p->setBrush(QColor("#0a0a0a"));
        p->drawEllipse(c, travel * 1.4, travel * 1.4);

        QPointF stick = c + QPointF(dx * travel, -dy * travel);
        p->setPen(QPen(QColor("#333"), 3));
        p->drawLine(c, stick);

        QColor cap = pressed_ ? QColor("#e63946") : JOY_ACTIVE;
        p->setPen(QPen(cap.darker(160), 2));
        p->setBrush(cap);
        p->drawEllipse(stick, 13, 13);

        // Straight leads on the right edge, one per VRX/VRY/SW slot; wire i attaches at
        // local (width, 15 + i*5), matching WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 3; ++i) {
            qreal y = 15 + i * 5;
            p->drawLine(QPointF(r.width() - 5, y), QPointF(r.width(), y));
        }
    }

    // Right-click simulates pressing the stick down (SW); left-drag moves X/Y.
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::RightButton) {
            pressed_ = true;
            if (swPin_ >= 0)
                emit inputChanged(swPin_, (int)ComponentEventType::DigitalPress, 0);
        } else {
            dragging_ = true;
            dragStartPos_ = event->pos();
        }
        update();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (!dragging_) return;
        QPointF delta = event->pos() - dragStartPos_;
        vrx_ = qBound(0, (int)(512 + delta.x() * 8), 1023);
        vry_ = qBound(0, (int)(512 - delta.y() * 8), 1023);
        emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, vrx_);
        if (vryPin_ >= 0)
            emit inputChanged(vryPin_, (int)ComponentEventType::AnalogValue, vry_);
        update();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::RightButton) {
            pressed_ = false;
            if (swPin_ >= 0)
                emit inputChanged(swPin_, (int)ComponentEventType::DigitalPress, 1);
        } else if (dragging_) {
            dragging_ = false;
            vrx_ = 512;
            vry_ = 512;
            emit inputChanged(pin(), (int)ComponentEventType::AnalogValue, vrx_);
            if (vryPin_ >= 0)
                emit inputChanged(vryPin_, (int)ComponentEventType::AnalogValue, vry_);
        }
        update();
    }
};

static bool reg_joystick = []() {
    ComponentDefinition def{
        "Joystick",
        {"JOYSTICK", "JOY", "VRX", "VRY"},
        {
            {"VRX", {"VRX"}},
            {"VRY", {"VRY"}},
            {"SW",  {"SW"}},
        },
        {},
        false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new JoystickItem(pin, parent);
        },
        MultiPinStrategy::Suffix,
        "VRX"
    };
    def.wire_color = JOY_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
