#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <cmath>

static const QColor ROTENC_ACTIVE("#1f7a52");

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

        // Leads drawn under the knob, extending past its edge, so stubs stay flush regardless
        // of radius; wire i attaches at local (width, 15 + i*5), matching WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 2; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(r.width() - 50, ly), QPointF(r.width(), ly));
        }

        qreal rad = qMin(r.width(), r.height()) * 0.45;
        QPointF c(r.center().x() + 10, r.center().y());
        QColor ringColor = dragging_ ? ROTENC_ACTIVE.lighter(140) : ROTENC_ACTIVE;
        p->setPen(QPen(ringColor.darker(180), 3));
        p->setBrush(ringColor);
        p->drawEllipse(c, rad, rad);

        p->setPen(QPen(QColor("#111"), 1));
        int notches = 20;
        for (int i = 0; i < notches; ++i) {
            qreal a = qDegreesToRadians(360.0 * i / notches);
            QPointF p1(c.x() + std::cos(a) * rad * 0.95, c.y() + std::sin(a) * rad * 0.95);
            QPointF p2(c.x() + std::cos(a) * rad * 0.75, c.y() + std::sin(a) * rad * 0.75);
            p->drawLine(p1, p2);
        }

        // No clamping on the angle -- unlike the potentiometer's bounded 270deg sweep, an
        // encoder spins freely past 360deg.
        qreal a = qDegreesToRadians(value_ * 30.0);
        p->setPen(QPen(QColor("#1a1a1a"), 2));
        p->drawLine(c, c + QPointF(std::cos(a) * rad * 0.8, std::sin(a) * rad * 0.8));
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