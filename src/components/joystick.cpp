#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <cmath>

static const QColor JOY_ACTIVE  ("#74dcb5");
static const QColor JOY_INACTIVE("#103729");
static const QColor JOY_PRESSED_BORDER("#2555e6");

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
        qreal ratio = qBound(0.0, std::sqrt(dx * dx + dy * dy), 1.0);
        int r = JOY_INACTIVE.red()   + ratio * (JOY_ACTIVE.red()   - JOY_INACTIVE.red());
        int g = JOY_INACTIVE.green() + ratio * (JOY_ACTIVE.green() - JOY_INACTIVE.green());
        int b = JOY_INACTIVE.blue()  + ratio * (JOY_ACTIVE.blue()  - JOY_INACTIVE.blue());
        QColor fill(r, g, b);
        p->setPen(QPen(pressed_ ? JOY_PRESSED_BORDER : fill.darker(150), pressed_ ? 2 : 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft,
                    QString("Joy: %1,%2").arg(vrx_).arg(vry_));
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
