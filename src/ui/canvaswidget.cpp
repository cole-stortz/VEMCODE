#include "src/ui/canvaswidget.h"
#include "src/core/circuit/componentregistry.h"
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <algorithm>
#include <map>

// Canvas color palette
static const QColor COLOR_BOARD_BG       ("#1a1a2e");
static const QColor COLOR_BOARD_BORDER   ("#3a3a5c");
static const QColor COLOR_CHIP_BG        ("#0d1117");
static const QColor COLOR_CHIP_BORDER    ("#2a2a4c");
static const QColor COLOR_BOARD_LABEL    ("#555577");
static const QColor COLOR_CHIP_LABEL     ("#333355");
static const QColor COLOR_PIN_DOT_BG     ("#2a2a3a");
static const QColor COLOR_PIN_DOT_BORDER ("#444466");
static const QColor COLOR_PIN_LABEL      ("#333355");


static const QColor COLOR_COMPONENT_LABEL     ("#cccccc");
static const QColor COLOR_COMPONENT_SUBLABEL  ("#888888");

CanvasWidget::CanvasWidget(QWidget* parent)
    : QGraphicsView(parent)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing);
    setStyleSheet("background: #1a1a1a; border: none;");
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    drawBoard();
}

void CanvasWidget::refresh(const std::vector<DetectedComponent>& components) {
    scene_->clear();
    pinItems_.clear();

    drawBoard();

    // Phase 1: create every item and work out its pin-aligned target Y --
    // boundingRect() varies per component type (44/54/64px), so heights can
    // only be known after construction.
    struct Placement {
        const DetectedComponent* comp;
        const ComponentDefinition* def;
        ComponentItem* item;
        float comp_x;
        int comp_w, comp_h;
        float target_y;
    };

    // Without this, the two left-side columns land on the same X when every
    // component is the same width (100px), since comp_w cancels out.
    static constexpr float H_GAP = 12.0f;

    std::vector<Placement> placements;
    for (const auto& comp : components) {
        if (comp.pin < 0) continue;
        const ComponentDefinition* def = ComponentRegistry::instance().find_by_type(comp.type_name);
        if (!def) continue;

        ComponentItem* item = def->create_item(comp.pin, nullptr);
        QRectF box = item->boundingRect();
        int comp_w = (int)box.width();
        int comp_h = (int)box.height();

        QPointF pin_pos = pinLocation(comp.pin);
        bool is_output = def->is_output;
        bool is_analog_input = isAnalogPin(comp.pin);

        float comp_x;
        if (is_output) {
            comp_x = BOARD_X + BOARD_W + 80;
        } else if (is_analog_input) {
            comp_x = BOARD_X - comp_w - 180 - H_GAP;  // outer column -- all analog
        } else {
            comp_x = BOARD_X - comp_w - 80;   // inner column -- digital inputs
        }
        float target_y = pin_pos.y() - comp_h / 2.0f;

        placements.push_back({&comp, def, item, comp_x, comp_w, comp_h, target_y});
    }

    // Phase 2: stack each column top-to-bottom, pushing components down past
    // the previous one's bottom edge so tightly-packed pins don't overlap.
    std::stable_sort(placements.begin(), placements.end(),
        [](const Placement& a, const Placement& b) {
            if (a.comp_x != b.comp_x) return a.comp_x < b.comp_x;
            return a.target_y < b.target_y;
        });

    static constexpr float V_GAP = 12.0f;
    std::map<float, float> column_bottom_y; // comp_x -> bottom edge of last placed item
    for (auto& p : placements) {
        float comp_y = p.target_y;
        auto it = column_bottom_y.find(p.comp_x);
        if (it != column_bottom_y.end())
            comp_y = std::max(comp_y, it->second + V_GAP);
        column_bottom_y[p.comp_x] = comp_y + p.comp_h;

        placeComponent(*p.comp, p.def, p.item, p.comp_x, comp_y, p.comp_w, p.comp_h);
    }
}

void CanvasWidget::updatePin(int pin, int value) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;
    it.value()->onPinChanged(pin, value);
}

void CanvasWidget::updateLcdText(int pin, int row, const QString& text) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;
    it.value()->updateText(row, text);
}

void CanvasWidget::onComponentInput(int pin, int eventType, QVariant value) {
    emit inputChanged(pin, eventType, value);
}

void CanvasWidget::drawBoard() {
    scene_->addRect(
        BOARD_X, BOARD_Y, BOARD_W, BOARD_H,
        QPen(COLOR_BOARD_BORDER, 2),
        QBrush(COLOR_BOARD_BG)
    );

    QGraphicsTextItem* label = scene_->addText(profile_.name);
    label->setDefaultTextColor(COLOR_BOARD_LABEL);
    label->setFont(QFont("Courier New", 9));
    label->setPos(BOARD_X + BOARD_W / 2.0 - 40, BOARD_Y + 10);

    scene_->addRect(
        BOARD_X + 40, BOARD_Y + 80, 120, 60,
        QPen(COLOR_CHIP_BORDER, 1),
        QBrush(COLOR_CHIP_BG)
    );
    QGraphicsTextItem* chipLabel = scene_->addText(profile_.chip);
    chipLabel->setDefaultTextColor(COLOR_CHIP_LABEL);
    chipLabel->setFont(QFont("Courier New", 8));
    chipLabel->setPos(BOARD_X + 48, BOARD_Y + 100);

    // Digital pins -- both below the analog block and (for boards like
    // Teensy 4.1) any extra digital pins above it
    for (int i = 0; i < profile_.pin_count; i++) {
        if (isAnalogPin(i)) continue;
        QPointF pos = pinLocation(i);
        scene_->addEllipse(
            pos.x() - 3, pos.y() - 3, 6, 6,
            QPen(COLOR_PIN_DOT_BORDER, 1),
            QBrush(COLOR_PIN_DOT_BG)
        );
        QGraphicsTextItem* pinNum = scene_->addText(QString::number(i));
        pinNum->setDefaultTextColor(COLOR_PIN_LABEL);
        pinNum->setFont(QFont("Courier New", 7));
        pinNum->setPos(pos.x() + 6, pos.y() - 8);
    }

    // Analog pins
    for (int i = profile_.analog_offset; i < profile_.analog_offset + profile_.analog_count; i++) {
        QPointF pos = pinLocation(i);
        scene_->addEllipse(
            pos.x() - 3, pos.y() - 3, 6, 6,
            QPen(COLOR_PIN_DOT_BORDER, 1),
            QBrush(COLOR_PIN_DOT_BG)
        );
        QGraphicsTextItem* pinNum = scene_->addText(QString("A%1").arg(i - 14));
        pinNum->setPlainText(QString("A%1").arg(i - profile_.analog_offset));
        pinNum->setDefaultTextColor(COLOR_PIN_LABEL);
        pinNum->setFont(QFont("Courier New", 7));
        pinNum->setPos(pos.x() - 24, pos.y() - 8);
    }
}

void CanvasWidget::placeComponent(const DetectedComponent& comp, const ComponentDefinition* def,
                                   ComponentItem* item, float comp_x, float comp_y,
                                   int comp_w, int comp_h) {
    bool is_output = def->is_output;

    item->setPos(comp_x, comp_y);
    scene_->addItem(item);

    // Connect BEFORE anything can emit -- configureMultiPin/emitInitialValue
    // below may synchronously emit inputChanged, and Qt drops signals emitted
    // with nothing connected yet rather than buffering them.
    connect(item, &ComponentItem::inputChanged, this, &CanvasWidget::onComponentInput);

    pinItems_[comp.pin] = item;
    if (comp.pins.size() > 1) {
        for (int mp : comp.pins) {
            if (mp >= 0)
                pinItems_[mp] = item;
        }
        item->configureMultiPin(comp.pins);
    }
    item->emitInitialValue();

    
    QGraphicsTextItem* typeText = new QGraphicsTextItem(item);
    typeText->setPlainText(QString::fromStdString(comp.label));
    typeText->setDefaultTextColor(COLOR_COMPONENT_SUBLABEL);
    typeText->setFont(QFont("Courier New", 8));
    typeText->setPos(6, -16);

    // if (!comp.pin_name.empty()) {
    //     QGraphicsTextItem* nameText = new QGraphicsTextItem(item);
    //     nameText->setPlainText(QString::fromStdString(comp.pin_name));
    //     nameText->setDefaultTextColor(COLOR_COMPONENT_SUBLABEL);
    //     nameText->setFont(QFont("Courier New", 7));
    //     nameText->setPos(6, -16);
    // }

    std::vector<int> wire_pins;
    if (!comp.pins.empty())
        wire_pins = comp.pins;
    else
        wire_pins = { comp.pin };

    const QColor& wireColor = def->wire_color;

    int i = 0;
    for (int wpin : wire_pins) {
        if (wpin < 0) { i++; continue; }

        QPointF target = pinLocation(wpin);

        // Each wire attaches at a different y so they don't stack
        float attach_y = comp_y + 15.0f + i * WIRE_SPACING;

        QPointF comp_edge = is_output
            ? QPointF(comp_x, attach_y)
            : QPointF(comp_x + comp_w, attach_y);

        bool pin_is_analog = isAnalogPin(wpin);
        if (pin_is_analog) {
            // Turn near the pin (in the gap before the digital column), not
            // near the destination -- otherwise the vertical segment cuts
            // through whatever got stacked between the pin and its component.
            float inter_x = BOARD_X - 20.0f - i * WIRE_SPACING;
            QPointF mid1(inter_x, target.y());
            QPointF mid2(inter_x, attach_y);
            drawWire(target, mid1, wireColor);
            drawWire(mid1, mid2, wireColor);
            drawWire(mid2, comp_edge, wireColor);
        } else if (is_output) {
            // Digital output: component sits on the same side as the pin, so
            // there's no column to cross -- turning near the component is fine.
            float inter_x = comp_x - 10.0f - i * WIRE_SPACING;
            QPointF mid1(inter_x, target.y());
            QPointF mid2(inter_x, attach_y);
            drawWire(target, mid1, wireColor);
            drawWire(mid1, mid2, wireColor);
            drawWire(mid2, comp_edge, wireColor);
        } else {
            // Digital input: same fix as analog, mirrored -- turn right next
            // to the pin (board's right edge) instead of next to the component.
            float inter_x = target.x() + 10.0f + i * WIRE_SPACING;
            QPointF mid1(inter_x, target.y());
            QPointF mid2(inter_x, attach_y);
            drawWire(target, mid1, wireColor);
            drawWire(mid1, mid2, wireColor);
            drawWire(mid2, comp_edge, wireColor);
        }
        i++;
    }
}

void CanvasWidget::drawWire(QPointF from, QPointF to, const QColor& color) {
    scene_->addLine(
        from.x(), from.y(), to.x(), to.y(),
        QPen(color, 1, Qt::SolidLine, Qt::RoundCap)
    );
}

bool CanvasWidget::isAnalogPin(int pin) const {
    return pin >= profile_.analog_offset && pin < profile_.analog_offset + profile_.analog_count;
}

int CanvasWidget::digitalPinCount() const {
    int low  = profile_.analog_offset;
    int high = std::max(0, profile_.pin_count - (profile_.analog_offset + profile_.analog_count));
    return low + high;
}

int CanvasWidget::digitalPinIndex(int pin) const {
    if (pin < 0 || pin >= profile_.pin_count || isAnalogPin(pin)) return -1;
    if (pin < profile_.analog_offset) return pin;
    int high_start = profile_.analog_offset + profile_.analog_count;
    return profile_.analog_offset + (pin - high_start);
}

QPointF CanvasWidget::pinLocation(int pin) {
    if (isAnalogPin(pin)) {
        float spacing = (float)BOARD_H / (float)(profile_.analog_count + 1);
        float y = BOARD_Y + spacing * (pin - profile_.analog_offset + 1);
        return QPointF(BOARD_X, y);
    }
    int idx = digitalPinIndex(pin);
    if (idx >= 0) {
        float spacing = (float)BOARD_H / (float)(digitalPinCount() + 1);
        float y = BOARD_Y + spacing * (idx + 1);
        return QPointF(BOARD_X + BOARD_W, y);
    }
    return QPointF(BOARD_X + BOARD_W / 2.0, BOARD_Y);
}