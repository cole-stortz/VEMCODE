#include "src/ui/canvaswidget.h"
#include "src/core/circuit/componentregistry.h"
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>
#include <QFont>

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

// Wire color
static const QColor COLOR_WIRE           ("#444466");

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

    for (const auto& comp : components) {
        if (comp.pin >= 0)
            drawComponent(comp);
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

    // Digital pins
    for (int i = 0; i < profile_.analog_offset; i++) {
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

void CanvasWidget::drawComponent(const DetectedComponent& comp) {
    const ComponentDefinition* def = ComponentRegistry::instance().find_by_type(comp.type_name);
    if (!def) return;

    ComponentItem* item = def->create_item(comp.pin, nullptr);

    QRectF box = item->boundingRect();
    int comp_w = (int)box.width();
    int comp_h = (int)box.height();

    QPointF pin_pos = pinLocation(comp.pin);
    bool is_output = def->is_output;
    bool is_analog_input = (comp.pin >= profile_.analog_offset);

    float comp_x;
    if (is_output) {
        comp_x = BOARD_X + BOARD_W + 80;
    } else if (is_analog_input) {
        comp_x = BOARD_X - comp_w - 180;  // outer column -- all analog
    } else {
        comp_x = BOARD_X - comp_w - 80;   // inner column -- digital inputs
    }
    // Align Y with the actual pin position on the board
    float comp_y = pin_pos.y() - comp_h / 2.0f;

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

    // Descriptive label -- sits above the box so it doesn't collide with the
    // item's own internal label drawn in its paint()
    QGraphicsTextItem* typeText = new QGraphicsTextItem(item);
    typeText->setPlainText(QString::fromStdString(comp.label));
    typeText->setDefaultTextColor(COLOR_COMPONENT_LABEL);
    typeText->setFont(QFont("Courier New", 8));
    typeText->setPos(6, -16);

    if (!comp.pin_name.empty()) {
        QGraphicsTextItem* nameText = new QGraphicsTextItem(item);
        nameText->setPlainText(QString::fromStdString(comp.pin_name));
        nameText->setDefaultTextColor(COLOR_COMPONENT_SUBLABEL);
        nameText->setFont(QFont("Courier New", 7));
        nameText->setPos(6, -30);
    }

    std::vector<int> wire_pins;
    if (!comp.pins.empty())
        wire_pins = comp.pins;
    else
        wire_pins = { comp.pin };

    int i = 0;
    for (int wpin : wire_pins) {
        if (wpin < 0) { i++; continue; }

        QPointF target = pinLocation(wpin);

        // Each wire attaches at a different y so they don't stack
        float attach_y = comp_y + 15.0f + i * WIRE_SPACING;

        QPointF comp_edge = is_output
            ? QPointF(comp_x, attach_y)
            : QPointF(comp_x + comp_w, attach_y);

        bool pin_is_analog = (wpin >= profile_.analog_offset);
        if (pin_is_analog) {
            // Analog: 3-segment route around board left edge, staggered per wire
            float inter_x = BOARD_X - 80 - i * WIRE_SPACING;
            QPointF mid1(inter_x, target.y());
            QPointF mid2(inter_x, attach_y);
            drawWire(target, mid1);
            drawWire(mid1, mid2);
            drawWire(mid2, comp_edge);
        } else {
            // Digital: horizontal from pin, vertical at staggered x, horizontal into component
            float inter_x = is_output
                ? comp_x - 10.0f - i * WIRE_SPACING
                : comp_x + comp_w + 10.0f + i * WIRE_SPACING;
            QPointF mid1(inter_x, target.y());
            QPointF mid2(inter_x, attach_y);
            drawWire(target, mid1);
            drawWire(mid1, mid2);
            drawWire(mid2, comp_edge);
        }
        i++;
    }
}

void CanvasWidget::drawWire(QPointF from, QPointF to) {
    scene_->addLine(
        from.x(), from.y(), to.x(), to.y(),
        QPen(COLOR_WIRE, 1, Qt::SolidLine, Qt::RoundCap)
    );
}

QPointF CanvasWidget::pinLocation(int pin) {
    if (pin >= 0 && pin < profile_.analog_offset) {
        float spacing = (float)BOARD_H / (float)(profile_.analog_offset + 1);
        float y = BOARD_Y + spacing * (pin + 1);
        return QPointF(BOARD_X + BOARD_W, y);
    }
    if (pin >= profile_.analog_offset && pin < profile_.analog_offset + profile_.analog_count) {
        float spacing = (float)BOARD_H / (float)(profile_.analog_count + 1);
        float y = BOARD_Y + spacing * (pin - profile_.analog_offset + 1);
        return QPointF(BOARD_X, y);
    }
    return QPointF(BOARD_X + BOARD_W / 2.0, BOARD_Y);
}