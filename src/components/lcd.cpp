#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include "src/core/circuit/circuitdetector.h"
#include <QPainter>
#include <regex>
#include <algorithm>

static const QColor LCD_FILL("#c26ce8");

class LCDItem : public ComponentItem {
    static constexpr int MAX_ROWS = 4;
    QString rowText_[MAX_ROWS];
    int numCols_ = 16;
    int numRows_ = 2;
    bool i2c_ = true; // flipped to false by configureMultiPin (parallel wiring only)

public:
    LCDItem(int p, QGraphicsItem* parent) : ComponentItem(p, parent) {
        for (auto& r : rowText_) r = QString(numCols_, ' ');
    }

    // Only real LCD sizes apply -- every component gets a default configureDisplaySize(128,
    // 64) call (OLED's default), which is out of range here and ignored.
    void configureDisplaySize(int width, int height) override {
        if (width < 1 || width > 20 || height < 1 || height > MAX_ROWS) return;
        numCols_ = width;
        numRows_ = height;
        for (auto& r : rowText_) r = QString(numCols_, ' ');
    }

    void configureMultiPin(const std::vector<int>&) override { i2c_ = false; }

    QRectF boundingRect() const override {
        qreal w = 100.0 * (numCols_ > 16 ? (qreal)numCols_ / 16.0 : 1.0);
        qreal h = 54.0 * (numRows_ > 2 ? (qreal)numRows_ / 2.0 : 1.0);
        return QRectF(0, 0, w, h);
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        QColor housing = themedHousing(LCD_FILL, isDarkTheme(), 400);
        p->setPen(QPen(housing.darker(180), 3));
        p->setBrush(housing);
        p->drawRoundedRect(r, 4, 4);

        QRectF screen = r.adjusted(r.width() * 0.06, r.height() * 0.18, -r.width() * 0.06, -r.height() * 0.1);
        p->setPen(QPen(QColor("#0a2a1a"), 1));
        p->setBrush(QColor("#4ecb71"));
        p->drawRect(screen);

        p->setPen(QColor("#0a2a1a"));
        p->setFont(QFont("Courier New", 7));
        qreal rowH = screen.height() / numRows_;
        for (int i = 0; i < numRows_; ++i) {
            p->drawText(QRectF(screen.left() + 4, screen.top() + i * rowH, screen.width() - 8, rowH),
                        Qt::AlignLeft | Qt::AlignVCenter, rowText_[i].left(numCols_));
        }

        // Straight leads on the left edge -- 6 (RS/EN/D4-D7) when parallel-wired, 1 (shared
        // I2C bus) otherwise; wire i attaches at local (0, 15 + i*5), matching WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        int leadCount = i2c_ ? 1 : 6;
        for (int i = 0; i < leadCount; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(3, ly), QPointF(0, ly));
        }
    }

    void updateText(int row, const QString& text) override {
        if (row < 0 || row >= MAX_ROWS) return;
        rowText_[row] = text.left(numCols_).leftJustified(numCols_);
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

    // "LiquidCrystal_I2C lcd(addr, cols, rows)" -- I2C has no dedicated GPIO pin, and the
    // generic Singleton-role engine above only covers the parallel-wired variant.
    def.detect_custom = [](CircuitDetector& ctx, const std::string& source,
                            const std::map<std::string, std::string>& defines,
                            const std::map<std::string, std::vector<int>>&,
                            std::set<int>& claimed) {
        static const std::regex ctor_re(
            R"(\bLiquidCrystal_I2C\s+(\w+)\s*(?:=\s*LiquidCrystal_I2C\s*)?\(\s*(\w+)\s*(?:,\s*(\w+)\s*,\s*(\w+)\s*)?\))");

        for (auto it = std::sregex_iterator(source.begin(), source.end(), ctor_re);
             it != std::sregex_iterator(); ++it) {
            std::string obj_name = (*it)[1].str();

            int cols = 16, rows = 2;
            if (it->size() > 4 && (*it)[4].matched) {
                int c = ctx.resolve_pin((*it)[3].str(), defines);
                int r = ctx.resolve_pin((*it)[4].str(), defines);
                if (c > 0) cols = std::min(c, 20);
                if (r > 0) rows = std::min(r, 4);
            }

            int pin_key = ctx.next_i2c_bus_pin();
            if (claimed.count(pin_key) || ctx.pin_already_added(pin_key)) continue;

            DetectedComponent comp;
            comp.type_name       = "LCD";
            comp.pin             = pin_key;
            comp.pins             = {pin_key};
            comp.pin_name         = obj_name;
            comp.confirmed        = false;
            comp.display_width    = cols;
            comp.display_height   = rows;
            comp.label = "LCD " + obj_name + " (I2C, " + std::to_string(cols) + "x" + std::to_string(rows) + ")";

            ctx.add_detected_component(comp, claimed);
        }
    };

    ComponentRegistry::instance().register_component(def);
    return true;
}();
