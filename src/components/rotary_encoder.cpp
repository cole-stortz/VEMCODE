#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>

static const QColor ROTENC_ACTIVE  ("#81dc74");
static const QColor ROTENC_INACTIVE("#153710");

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
        QColor fill = dragging_ ? ROTENC_ACTIVE : ROTENC_INACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, QString("Rotary Enc: %1").arg(value_));
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