#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QtMath>
#include <cmath>

static const QColor STEPPER_ACTIVE("#8e5c2e");

// Handles two real-world wiring styles with the same class: STEP+DIR (driver
// boards like A4988/DRV8825) and IN1-IN4 (ULN2003 + 28BYJ-48 style, driven by
// bit-banging the 4 phase pins directly). configureMultiPin picks the mode
// from how many pins were detected.
class StepperItem : public ComponentItem {
    int dirPin_ = -1;
    int phasePins_[4] = {-1, -1, -1, -1};
    bool phaseState_[4] = {false, false, false, false};
    int lastPhaseIdx_ = -1;
    bool fourPhase_ = false;
    bool dirCW_ = true;
    long position_ = 0;
    bool stepHigh_ = false;

public:
    StepperItem(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();

        // Leads drawn under the body, extending well past its edge -- the
        // body paints over the overlap, so the visible stubs always end up
        // flush with the body's edge regardless of the exact radius.
        // Steppers are outputs, so CanvasWidget::updateWires attaches wire i
        // at local (0, 15 + i*5), same spacing as WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        int leadCount = fourPhase_ ? 4 : 2;
        for (int i = 0; i < leadCount; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(10, ly), QPointF(0, ly));
        }

        // Matches HBridgeMotor's absolute radius (54 * 0.42) rather than
        // deriving from this box's own (shorter, 44px) height, so the two
        // read as the same size despite Stepper's bounding rect being
        // shorter.
        qreal rad = 54.0 * 0.42;
        QPointF c(r.left() + rad + 6, r.center().y());
        p->setPen(QPen(STEPPER_ACTIVE, 3));
        p->setBrush(STEPPER_ACTIVE.darker(200));
        p->drawEllipse(c, rad, rad);

        p->setPen(QPen(QColor("#222"), 1));
        p->setBrush(QColor("#555"));
        p->drawEllipse(c, rad * 0.18, rad * 0.18);

        // Clock-style arm instead of a directional arc -- steppers just
        // track an accumulated position (15deg/step), not a continuous
        // spin rate.
        qreal a = qDegreesToRadians((double)(position_ * 15));
        p->setPen(QPen(QColor("#1a1a1a"), 3, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(c, c + QPointF(std::cos(a) * rad * 0.8, std::sin(a) * rad * 0.8));

        p->setPen(STEPPER_ACTIVE.lighter(200));
        p->setFont(QFont("Courier New", 7));
        qreal textX = r.width() * 0.6;
        qreal textW = r.width() - textX;
        p->drawText(QRectF(textX, r.center().y() - 14, textW, 14), Qt::AlignCenter, QString("pos:%1").arg(position_));
        p->drawText(QRectF(textX, r.center().y(), textW, 14), Qt::AlignCenter, dirCW_ ? "CW" : "CCW");
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() == 2) {
            dirPin_ = pins[1];
        } else if (pins.size() == 4) {
            fourPhase_ = true;
            for (int i = 0; i < 4; ++i) phasePins_[i] = pins[i];
        }
    }

    void onPinChanged(int pin, int value) override {
        if (fourPhase_) {
            for (int i = 0; i < 4; ++i)
                if (phasePins_[i] == pin) { phaseState_[i] = value != 0; break; }

            int highCount = 0, highIdx = -1;
            for (int i = 0; i < 4; ++i)
                if (phaseState_[i]) { highCount++; highIdx = i; }

            // Only advance on a clean single-phase-energized state -- this
            // naturally ignores half-step transitional states (2 bits on)
            // without needing to hardcode any particular coil sequence table.
            if (highCount == 1) {
                if (lastPhaseIdx_ >= 0 && highIdx != lastPhaseIdx_) {
                    if (highIdx == (lastPhaseIdx_ + 1) % 4)      { position_++; dirCW_ = true; }
                    else if (highIdx == (lastPhaseIdx_ + 3) % 4) { position_--; dirCW_ = false; }
                }
                lastPhaseIdx_ = highIdx;
            }
            update();
            return;
        }

        if (pin == dirPin_) {
            dirCW_ = value != 0;
            update();
            return;
        }
        // STEP pin (or the lone pin in single-pin fallback): count rising edges
        stepHigh_ = value != 0;
        if (stepHigh_) position_ += dirCW_ ? 1 : -1;
        update();
    }
};

static bool registered_stepper = []() {
    ComponentDefinition stepDir{
        "Stepper",
        {"STEPPER", "STEP", "DIR"},
        {
            {"STEP", {"STEP"}},
            {"DIR",  {"DIR"}},
        },
        {},
        true, // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new StepperItem(pin, parent);
        },
        MultiPinStrategy::Singleton,
        "STEP"
    };
    stepDir.wire_color = STEPPER_ACTIVE;
    ComponentRegistry::instance().register_component(stepDir);

    // Separate entry for the IN1-IN4 wiring style -- same type_name and
    // create_item, just a different detect_multi role set. Both entries are
    // tried independently by CircuitDetector; whichever pin set actually
    // exists in the sketch wins. Note: bare "IN1".."IN4" substring-match
    // against any coincidentally similar name (e.g. "PIN1".."PIN4"), same
    // keyword-collision tradeoff already accepted elsewhere in this registry
    // (see HBridgeMotor's "IN1" keyword) -- only fires if all four are present.
    ComponentDefinition phase4{
        "Stepper",
        {},
        {
            {"IN1", {"IN1"}},
            {"IN2", {"IN2"}},
            {"IN3", {"IN3"}},
            {"IN4", {"IN4"}},
        },
        {},
        true,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new StepperItem(pin, parent);
        },
        MultiPinStrategy::Singleton,
        "IN1"
    };
    phase4.wire_color = STEPPER_ACTIVE;
    ComponentRegistry::instance().register_component(phase4);
    return true;
}();
