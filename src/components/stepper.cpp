#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>

static const QColor STEPPER_ACTIVE  ("#dcb574");
static const QColor STEPPER_INACTIVE("#372a10");

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

public:
    StepperItem(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = STEPPER_ACTIVE;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft,
                    QString("Stepper\npos: %1  %2").arg(position_).arg(dirCW_ ? "CW" : "CCW"));
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
                update();
            }
            return;
        }

        if (pin == dirPin_) {
            dirCW_ = value != 0;
            update();
            return;
        }
        // STEP pin (or the lone pin in single-pin fallback): count rising edges
        if (value != 0) position_ += dirCW_ ? 1 : -1;
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
