#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QLinearGradient>

static const QColor LCD_FILL("#cf74dc");

class LCDItem : public ComponentItem {
    QString row0_ = QString(16, ' ');
    QString row1_ = QString(16, ' ');

public:
    LCDItem(int p, QGraphicsItem* parent) : ComponentItem(p, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 54); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        QColor housing("#2a2a2a");
        QLinearGradient bg(r.topLeft(), r.bottomLeft());
        bg.setColorAt(0.0, housing.lighter(130));
        bg.setColorAt(0.5, housing);
        bg.setColorAt(1.0, housing.darker(120));
        p->setPen(QPen(housing.darker(180), 1.2));
        p->setBrush(bg);
        p->drawRoundedRect(r, 4, 4);

        QRectF screen = r.adjusted(r.width() * 0.06, r.height() * 0.18, -r.width() * 0.06, -r.height() * 0.1);
        p->setPen(QPen(QColor("#0a2a1a"), 1));
        p->setBrush(QColor("#4ecb71"));
        p->drawRect(screen);

        p->setPen(QColor("#0a2a1a"));
        p->setFont(QFont("Courier New", 7));
        p->drawText(QRectF(screen.left() + 4, screen.top() + 2, screen.width() - 8, screen.height() / 2 - 2),
                    Qt::AlignLeft | Qt::AlignVCenter, row0_.left(16));
        p->drawText(QRectF(screen.left() + 4, screen.top() + screen.height() / 2, screen.width() - 8, screen.height() / 2 - 2),
                    Qt::AlignLeft | Qt::AlignVCenter, row1_.left(16));

        p->setPen(QPen(QColor("#c0c0c0"), 1));
        for (int i = 0; i < 6; ++i) {
            qreal px = r.left() + 6 + i * (r.width() - 12) / 5.0;
            p->drawLine(QPointF(px, r.top()), QPointF(px, r.top() + 4));
        }

        // Straight leads on the left edge, one per RS/EN/D4-D7 pin slot --
        // LCDs are outputs, so CanvasWidget::updateWires attaches wire i at
        // local (0, 15 + i*5), same spacing as WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 6; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(10, ly), QPointF(0, ly));
        }
    }

    void updateText(int row, const QString& text) override {
        (row == 0 ? row0_ : row1_) = text.left(16).leftJustified(16);
        update();
    }
};

static bool registered = []() {
    ComponentDefinition def{
        "LCD",
        {"LCD"},
        {
            {"RS", {"RS"}},
            {"EN", {"LCD_EN", "LCD_E", "_ENABLE", "_EN"}},
            {"D4", {"D4", "DB4", "DATA4"}},
            {"D5", {"D5", "DB5", "DATA5"}},
            {"D6", {"D6", "DB6", "DATA6"}},
            {"D7", {"D7", "DB7", "DATA7"}},
        },
        {"LiquidCrystal"},
        true, // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new LCDItem(pin, parent);
        },
        MultiPinStrategy::Singleton,
        "RS"
    };
    def.wire_color = LCD_FILL;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
