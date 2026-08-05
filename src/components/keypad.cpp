#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include "src/core/circuit/circuitdetector.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QVariantList>
#include <regex>
#include <sstream>
#include <algorithm>

static const QColor KEY_ACTIVE  ("#e0822e");
static constexpr int CELL = 26;

// Real 4x4/4x3 membrane keypads (Arduino starter kit style) are silkscreened with this
// exact layout -- match it so the canvas grid reads like the physical part.
static QString keyLabelFor(int rows, int cols, int r, int c) {
    if (rows == 4 && cols == 4) {
        static const char* K[4][4] = {
            {"1", "2", "3", "A"}, {"4", "5", "6", "B"},
            {"7", "8", "9", "C"}, {"*", "0", "#", "D"},
        };
        return K[r][c];
    }
    if (rows == 4 && cols == 3) {
        static const char* K[4][3] = {
            {"1", "2", "3"}, {"4", "5", "6"},
            {"7", "8", "9"}, {"*", "0", "#"},
        };
        return K[r][c];
    }
    return QString::number(r * cols + c + 1);
}

class KeypadItem : public ComponentItem {
    int rows_ = 0, cols_ = 0;
    std::vector<int> rowPins_, colPins_;
    int pressedRow_ = -1, pressedCol_ = -1;

public:
    KeypadItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
    }

    QRectF boundingRect() const override {
        int r = rows_ > 0 ? rows_ : 4;
        int c = cols_ > 0 ? cols_ : 4;
        return QRectF(0, 0, c * CELL + 8, r * CELL + 6);
    }

    void configureRowsCols(int rows, int cols) override {
        rows_ = rows;
        cols_ = cols;
    }

    // Called after configureRowsCols, so rows_/cols_ are known; pins arrives as
    // [row_0..row_{rows-1}, col_0..col_{cols-1}].
    void configureMultiPin(const std::vector<int>& pins) override {
        if (rows_ <= 0 || cols_ <= 0 || (int)pins.size() < rows_ + cols_) return;
        rowPins_.assign(pins.begin(), pins.begin() + rows_);
        colPins_.assign(pins.begin() + rows_, pins.begin() + rows_ + cols_);
    }

    void emitInitialValue() override { emitWiring(); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        if (rows_ <= 0 || cols_ <= 0) return;
        bool dark = isDarkTheme();

        p->setPen(QPen(KEY_ACTIVE.darker(180), 3));
        p->setBrush(themedHousing(KEY_ACTIVE, dark, 400));
        p->drawRoundedRect(boundingRect(), 4, 4);

        p->setFont(QFont("Courier New", 9));
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                bool active = (r == pressedRow_ && c == pressedCol_);
                QRectF cell(4 + c * CELL, 4 + r * CELL, CELL - 2, CELL - 2);
                QColor fill = active ? KEY_ACTIVE : themedHousing(KEY_ACTIVE, dark, 400);
                p->setPen(QPen(fill.darker(160), 1));
                p->setBrush(fill);
                p->drawRoundedRect(cell, 3, 3);
                int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
                p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
                p->drawText(cell, Qt::AlignCenter, keyLabelFor(rows_, cols_, r, c));
            }
        }

        // Straight leads on the right edge, one per row/col pin slot, same order
        // configureMultiPin uses; wire i attaches at local (width, 15 + i*5), matching WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        QRectF r = boundingRect();
        for (int i = 0; i < rows_ + cols_; ++i) {
            qreal ly = 15 + i * 5;
            p->drawLine(QPointF(r.width() - 3, ly), QPointF(r.width() + 1, ly));
        }
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (rows_ <= 0 || cols_ <= 0) return;
        int c = (int)((event->pos().x() - 4) / CELL);
        int r = (int)((event->pos().y() - 4) / CELL);
        if (r < 0 || r >= rows_ || c < 0 || c >= cols_) return;
        pressedRow_ = r;
        pressedCol_ = c;
        update();
        // Addressed by actual pin number (not row/col index) so the runtime needs no
        // notion of "which keypad" -- pins are globally unique.
        emit inputChanged(pin(), (int)ComponentEventType::KeypadPress,
                           QVariantList{rowPins_[r], colPins_[c], 1});
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override {
        if (pressedRow_ < 0) return;
        emit inputChanged(pin(), (int)ComponentEventType::KeypadPress,
                           QVariantList{rowPins_[pressedRow_], colPins_[pressedCol_], 0});
        pressedRow_ = pressedCol_ = -1;
        update();
    }

private:
    // Payload: [numCols, col_pin_0.., row_pin_0..] -- pin numbers, not indices, so the
    // runtime stays keypad-instance-agnostic.
    void emitWiring() {
        QVariantList payload;
        payload << (int)colPins_.size();
        for (int p : colPins_) payload << p;
        for (int p : rowPins_) payload << p;
        emit inputChanged(pin(), (int)ComponentEventType::KeypadWiring, payload);
    }
};

static bool reg_keypad = []() {
    ComponentDefinition def{
        "Keypad",
        {"KEYPAD"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new KeypadItem(pin, parent);
        }
    };
    def.wire_color = KEY_ACTIVE;

    // Row/col counts are read straight from the sketch, not fixed, so this can't use the
    // fixed-role-count MultiPinStrategy engine.
    def.detect_custom = [](CircuitDetector& ctx, const std::string& source,
                            const std::map<std::string, std::string>& defines,
                            const std::map<std::string, std::vector<int>>&,
                            std::set<int>& claimed) {
        constexpr int MIN_LINES = 2, MAX_LINES = 4;
        if (source.find("Keypad") == std::string::npos) return;

        auto has_role_keyword = [](const std::string& upper, const char* kw) {
            return upper.find(kw) != std::string::npos;
        };

        // Preferred: rowPins[]/colPins[] arrays, the shape every real Keypad.h tutorial uses.
        auto scan_pin_array = [&](const char* keyword) -> std::vector<int> {
            static const std::regex arr_re(
                R"((?:const\s+)?(?:byte|int|uint8_t)\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{([^}]+)\})");
            for (auto it = std::sregex_iterator(source.begin(), source.end(), arr_re);
                 it != std::sregex_iterator(); ++it) {
                std::string name = (*it)[1].str();
                if (!has_role_keyword(ctx.to_upper(name), keyword)) continue;
                std::vector<int> pins;
                std::stringstream ss((*it)[2].str());
                std::string token;
                while (std::getline(ss, token, ',')) {
                    token.erase(0, token.find_first_not_of(" \t\r\n"));
                    token.erase(token.find_last_not_of(" \t\r\n") + 1);
                    int pin = ctx.resolve_pin(token, defines);
                    if (pin < 0) return {};
                    pins.push_back(pin);
                }
                return pins;
            }
            return {};
        };

        std::vector<int> row_pins = scan_pin_array("ROW");
        std::vector<int> col_pins = scan_pin_array("COL");

        // Fallback: grouped #defines -- #define ROW1 9 / ROW2 8 ... (same for COL).
        auto scan_defines = [&](const char* keyword) -> std::vector<int> {
            static const std::regex num_re(R"((\d+)$)");
            std::vector<std::pair<int,int>> numbered; // (suffix number, pin)
            for (const auto& d : defines) {
                std::string upper = ctx.to_upper(d.first);
                if (!has_role_keyword(upper, keyword)) continue;
                std::smatch m;
                if (!std::regex_search(upper, m, num_re)) continue;
                int pin = ctx.resolve_pin(d.second, defines);
                if (pin < 0) continue;
                numbered.push_back({std::stoi(m[1].str()), pin});
            }
            std::sort(numbered.begin(), numbered.end());
            std::vector<int> pins;
            for (auto& np : numbered) pins.push_back(np.second);
            return pins;
        };

        if (row_pins.empty()) row_pins = scan_defines("ROW");
        if (col_pins.empty()) col_pins = scan_defines("COL");

        int rows = (int)row_pins.size();
        int cols = (int)col_pins.size();
        if (rows < MIN_LINES || rows > MAX_LINES || cols < MIN_LINES || cols > MAX_LINES) return;

        for (int p : row_pins) if (claimed.count(p)) return;
        for (int p : col_pins) if (claimed.count(p)) return;

        DetectedComponent comp;
        comp.type_name = "Keypad";
        comp.pin       = row_pins[0];
        comp.pins      = row_pins;
        comp.pins.insert(comp.pins.end(), col_pins.begin(), col_pins.end());
        comp.rows      = rows;
        comp.cols      = cols;
        comp.pin_name  = "Keypad";
        comp.confirmed = false;

        std::string label = "Keypad (ROW=";
        for (size_t i = 0; i < row_pins.size(); ++i) label += (i ? "," : "") + std::to_string(row_pins[i]);
        label += " COL=";
        for (size_t i = 0; i < col_pins.size(); ++i) label += (i ? "," : "") + std::to_string(col_pins[i]);
        label += ")";
        comp.label = label;

        if (ctx.pin_already_added(comp.pin)) return;
        ctx.add_detected_component(comp, claimed);
    };

    ComponentRegistry::instance().register_component(def);
    return true;
}();
