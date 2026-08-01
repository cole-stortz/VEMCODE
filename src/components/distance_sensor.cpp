#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <cmath>

static const QColor DISTANCE_SENSOR_ACCENT("#2e9c8e");

class DistanceSensorItem : public ComponentItem {
    QLineEdit* input_;

public:
    DistanceSensorItem(int p, QGraphicsItem* parent)
        : ComponentItem(p, parent) {
        input_ = new QLineEdit();
        input_->setFixedSize(60, 20);
        input_->setPlaceholderText("cm");
        input_->setStyleSheet("background:#001a1a; color:#44ffff; border:1px solid #44ffff;");
        auto* proxy = new QGraphicsProxyWidget(this);
        proxy->setWidget(input_);
        proxy->setPos(34, 40);
        connect(input_, &QLineEdit::textChanged, this, [this](const QString& text) {
            bool ok;
            float cm = text.toFloat(&ok);
            if (ok && cm >= 0) {
                qulonglong micros = (qulonglong)std::ceil(cm * 2.0f / 0.034f);
                emit inputChanged(pin(), (int)ComponentEventType::PulseUs, micros);
            }
        });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 64); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        p->setPen(QPen(DISTANCE_SENSOR_ACCENT.darker(400), 3));
        p->setBrush(themedHousing(DISTANCE_SENSOR_ACCENT, isDarkTheme(), 600));
        p->drawRoundedRect(r, 4, 4);

        // "Eyes" live in the top band only -- the bottom is reserved for the
        // cm QLineEdit proxy, which is the actual human-readable readout.
        qreal eyeR = 16.0;
        QPointF c1(r.center().x() - eyeR * 1.25, r.top() + 22);
        QPointF c2(r.center().x() + eyeR * 1.25, r.top() + 22);
        for (const auto& c : {c1, c2}) {
            p->setPen(QPen(QColor("#222"), 1));
            p->setBrush(QColor("#9a9a9a"));
            p->drawEllipse(c, eyeR, eyeR);
            p->setPen(QPen(QColor("#9a9a9a").darker(500), 1));
            p->setBrush(QColor("#9a9a9a").darker(300));
            p->drawEllipse(c, eyeR * 0.8, eyeR * 0.8);
        }

        // Straight leads on the right edge, one per TRIG/ECHO pin slot --
        // distance sensors are inputs, so CanvasWidget::updateWires attaches
        // wire i at local (width, 15 + i*5), same spacing as WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 2; ++i) {
            qreal y = 15 + i * 5;
            p->drawLine(QPointF(r.width() - 5, y), QPointF(r.width(), y));
        }
    }

    // Called by CanvasWidget after inputChanged is connected -- setting this
    // in the constructor would fire textChanged before anything outside this
    // object could possibly be connected yet, silently dropping the signal.
    void emitInitialValue() override {
        input_->setText("10");
    }
};

static bool registered = []() {
    ComponentDefinition def{
        "DistanceSensor",
        {"TRIG", "ECHO", "DISTANCE", "ULTRASONIC", "SONAR", "HCSR"},
        {
            {"TRIG", {"TRIG"}},
            {"ECHO", {"ECHO"}},
        },
        {"pulseIn("},
        false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new DistanceSensorItem(pin, parent);
        },
        MultiPinStrategy::Suffix,
        "ECHO"
    };
    def.wire_color = DISTANCE_SENSOR_ACCENT;
    ComponentRegistry::instance().register_component(def);
    return true;
}();