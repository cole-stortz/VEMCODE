#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>


class RGBLedItem : public ComponentItem {
    int redpin_;
    int greenpin_ = -1;
    int bluepin_ = -1;

    int redval_ = 0;
    int greenval_ = 0;
    int blueval_ = 0;

public:
    RGBLedItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent), redpin_(pin) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 44); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill(redval_, greenval_, blueval_);
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 40), Qt::AlignLeft, "RGB LED");
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) greenpin_ = pins[1];
        if (pins.size() > 2) bluepin_ = pins[2];
    }

    void onPinChanged(int pin, int value) override {
        if (pin == redpin_)        redval_   = qBound(0, value, 255);
        else if (pin == greenpin_) greenval_ = qBound(0, value, 255);
        else if (pin == bluepin_)  blueval_  = qBound(0, value, 255);
        update();
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "RGBLED",
        {},
        {
            {"RED",   {"REDPIN", "RPIN", "RED_PIN", "R_PIN"}},
            {"GREEN", {"GREENPIN", "GPIN","GREEN_PIN", "G_PIN"}},
            {"BLUE",  {"BLUEPIN", "BPIN", "BLUE_PIN","B_PIN"}},
        },
        {},    // detect_pattern — none
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new RGBLedItem(pin, parent);
        },
        MultiPinStrategy::Suffix,
        "RED"
    });
    return true;
}();
