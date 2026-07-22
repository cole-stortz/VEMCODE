#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QVariantList>

static const QColor KEY_ACTIVE  ("#dc74c2");
static const QColor KEY_INACTIVE("#37102e");
static constexpr int CELL = 26;

// Real 4x4/4x3 membrane keypads (the ones in Arduino starter kits) are
// silkscreened with this exact layout -- match it so the canvas grid reads
// like the physical part, not an arbitrary numbering.
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
        return QRectF(0, 0, c * CELL + 8, r * CELL + 8);
    }

    void configureRowsCols(int rows, int cols) override {
        rows_ = rows;
        cols_ = cols;
    }

    // Called after configureRowsCols, so rows_/cols_ are already known --
    // pins arrives as [row_0..row_{rows-1}, col_0..col_{cols-1}].
    void configureMultiPin(const std::vector<int>& pins) override {
        if (rows_ <= 0 || cols_ <= 0 || (int)pins.size() < rows_ + cols_) return;
        rowPins_.assign(pins.begin(), pins.begin() + rows_);
        colPins_.assign(pins.begin() + rows_, pins.begin() + rows_ + cols_);
    }

    void emitInitialValue() override { emitWiring(); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        if (rows_ <= 0 || cols_ <= 0) return;
        p->setFont(QFont("Courier New", 9));
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                bool active = (r == pressedRow_ && c == pressedCol_);
                QRectF cell(4 + c * CELL, 4 + r * CELL, CELL - 2, CELL - 2);
                QColor fill = active ? KEY_ACTIVE : KEY_INACTIVE;
                p->setPen(QPen(fill.darker(150), 1));
                p->setBrush(fill);
                p->drawRect(cell);
                int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
                p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
                p->drawText(cell, Qt::AlignCenter, keyLabelFor(rows_, cols_, r, c));
            }
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
        // Addressed by actual pin number (not row/col index) so the runtime
        // side needs no notion of "which keypad" -- pins are globally unique.
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
    // Payload: [numCols, col_pin_0.., row_pin_0..] -- pin numbers, not indices,
    // so the runtime can key everything by pin and stay keypad-instance-agnostic.
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
    ComponentRegistry::instance().register_component(def);
    return true;
}();
