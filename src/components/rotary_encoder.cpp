#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <cmath>

static const QColor ROTENC_ACTIVE("#81dc74");

static const int QUAD_CLK[4] = {1, 0, 0, 1};
static const int QUAD_DT[4]  = {1, 1, 0, 0};
static const int STEP_PIXELS = 4;

class RotEncItem : public ComponentItem {
    bool dragging_ = false;
    int dragStartY_ = 0;
    int dragAccum_ = 0;
    int quadState_ = 0;
    int value_ = 0;
    int dtpin_ = -1;

public:
    RotEncItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::SizeVerCursor);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        QRectF body = r.adjusted(r.width() * 0.22, r.height() * 0.08, -r.width() * 0.22, -r.height() * 0.2);

        QColor housing("#263826");
        QLinearGradient bg(body.topLeft(), body.bottomLeft());
        bg.setColorAt(0.0, housing.lighter(130));
        bg.setColorAt(0.5, housing);
        bg.setColorAt(1.0, housing.darker(120));
        p->setPen(QPen(housing.darker(180), 1.2));
        p->setBrush(bg);
        p->drawRoundedRect(body, 4, 4);

        QPointF c = body.center();
        qreal rad = qMin(body.width(), body.height()) * 0.4;
        QColor ringColor = dragging_ ? ROTENC_ACTIVE : QColor("#3a3a3a");
        p->setPen(QPen(ringColor, 2));
        int notches = 20;
        for (int i = 0; i < notches; ++i) {
            qreal a = qDegreesToRadians(360.0 * i / notches);
            QPointF p1(c.x() + std::cos(a) * rad, c.y() + std::sin(a) * rad);
            QPointF p2(c.x() + std::cos(a) * rad * 0.85, c.y() + std::sin(a) * rad * 0.85);
            p->drawLine(p1, p2);
        }

        QRadialGradient knob(c - QPointF(rad * 0.25, rad * 0.25), rad * 1.3);
        knob.setColorAt(0.0, QColor("#4a4a4a"));
        knob.setColorAt(1.0, QColor("#1a1a1a"));
        p->setPen(QPen(QColor("#000"), 1));
        p->setBrush(knob);
        p->drawEllipse(c, rad * 0.7, rad * 0.7);

        qreal a = qDegreesToRadians((value_ * 30.0));
        p->setPen(QPen(ringColor, 2));
        p->drawLine(c, c + QPointF(std::cos(a) * rad * 0.6, std::sin(a) * rad * 0.6));

        // Straight leads on the right edge, one per CLK/DT pin slot -- rotary
        // encoders are inputs, so CanvasWidget::updateWires attaches wire i
        // at local (width, 15 + i*5), same spacing as WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 2; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(r.width() - 10, ly), QPointF(r.width(), ly));
        }
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) dtpin_ = pins[1];
    }

    void emitInitialValue() override {
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, QUAD_CLK[quadState_]);
        if (dtpin_ >= 0)
            emit inputChanged(dtpin_, (int)ComponentEventType::DigitalPress, QUAD_DT[quadState_]);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        dragging_ = true;
        dragStartY_ = event->pos().y();
        dragAccum_ = 0;
        update();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (!dragging_) return;
        int y = event->pos().y();
        dragAccum_ += dragStartY_ - y;
        dragStartY_ = y;

        while (dragAccum_ >= STEP_PIXELS) { dragAccum_ -= STEP_PIXELS; stepQuadrature(+1); }
        while (dragAccum_ <= -STEP_PIXELS) { dragAccum_ += STEP_PIXELS; stepQuadrature(-1); }
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override {
        dragging_ = false;
        update();
    }

private:
    void stepQuadrature(int dir) {
        quadState_ = (quadState_ + dir + 4) % 4;
        if (dtpin_ >= 0)
            emit inputChanged(dtpin_, (int)ComponentEventType::DigitalPress, QUAD_DT[quadState_]);
        emit inputChanged(pin(), (int)ComponentEventType::DigitalPress, QUAD_CLK[quadState_]);
        if (quadState_ == 0) value_ += dir;
        update();
    }
};

static bool reg_rotenc = []() {
    ComponentDefinition def{
        "RotaryEncoder",
        {"ENCODER", "ROTARY", "CLK", "DT"},
        {
            {"CLK", {"CLK"}},
            {"DT",  {"DT"}},
        },
        {},
        false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new RotEncItem(pin, parent);
        },
        MultiPinStrategy::Suffix,
        "CLK"
    };
    def.wire_color = ROTENC_ACTIVE;
    ComponentRegistry::instance().register_component(def);
    return true;
}();