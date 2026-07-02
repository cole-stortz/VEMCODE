#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QGraphicsTextItem>

static const QColor LCD_FILL("#3d0076");

class LCDItem : public ComponentItem {
    QGraphicsTextItem* row0_;
    QGraphicsTextItem* row1_;

public:
    LCDItem(int p, QGraphicsItem* parent)
        : ComponentItem(p, parent) {
        row0_ = new QGraphicsTextItem("                ", this);
        row0_->setDefaultTextColor(QColor("#cccccc"));
        row0_->setFont(QFont("Courier New", 7));
        row0_->setPos(6, 22);

        row1_ = new QGraphicsTextItem("                ", this);
        row1_->setDefaultTextColor(QColor("#cccccc"));
        row1_->setFont(QFont("Courier New", 7));
        row1_->setPos(6, 36);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 54); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill = LCD_FILL;
        p->setPen(QPen(fill.darker(150), 1));
        p->setBrush(fill);
        p->drawRect(boundingRect());
        p->setPen(QColor("#cccccc"));
        p->setFont(QFont("Courier New", 8));
        p->drawText(QRectF(6, 2, 88, 16), Qt::AlignLeft, "LCD");
    }

    void updateText(int row, const QString& text) override {
        QGraphicsTextItem* target = (row == 0) ? row0_ : row1_;
        target->setPlainText(text.left(16).leftJustified(16));
    }
};

static bool registered = []() {
    ComponentRegistry::instance().register_component({
        "LCD",
        {"LCD", "DISPLAY", "SCREEN", "OLED"},
        {
            {"RS", {"RS"}},
            {"EN", {"LCD_EN", "LCD_E", "_ENABLE", "_EN", "E", "EN"}},
            {"D4", {"D4", "DB4", "DATA4"}},
            {"D5", {"D5", "DB5", "DATA5"}},
            {"D6", {"D6", "DB6", "DATA6"}},
            {"D7", {"D7", "DB7", "DATA7"}},
        },
        {"LiquidCrystal"},
        true, // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new LCDItem(pin, parent);
        }
    });
    return true;
}();
