#include "src/ui/canvaswidget.h"
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <cmath>
#include <map>

CanvasWidget::CanvasWidget(QWidget* parent)
    : QGraphicsView(parent)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing);
    setStyleSheet("background: #1a1a1a; border: none;");
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Draw empty board on startup
    drawBoard();
}

// -------------------------------------------------------
// refresh() -- rebuilds the scene from detected components
// -------------------------------------------------------
void CanvasWidget::refresh(const std::vector<DetectedComponent>& components) {
    scene_->clear();
    pinItems_.clear();

    drawBoard();

    int index = 0;
    int total = 0;
    for (const auto& c : components)
        if (c.pin >= 0) total++;

    for (const auto& comp : components) {
        if (comp.pin >= 0) {
            drawComponent(comp, index, total);
            index++;
        }
    }
}

// -------------------------------------------------------
// updatePin() -- changes component color when pin fires
// -------------------------------------------------------
void CanvasWidget::updatePin(int pin, int value) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;

    // Find the component type for this pin to get the right active color
    // We store it in the item's data slot
    ComponentType type = static_cast<ComponentType>(it.value()->data(0).toInt());
    it.value()->setBrush(QBrush(componentColor(type, value == 1)));
}

// -------------------------------------------------------
// drawBoard() -- draws the Arduino Uno board
// -------------------------------------------------------
void CanvasWidget::drawBoard() {
    // Board body
    QGraphicsRectItem* board = scene_->addRect(
        BOARD_X, BOARD_Y, BOARD_W, BOARD_H,
        QPen(QColor("#3a3a5c"), 2),
        QBrush(QColor("#1a1a2e"))
    );

    // Board label
    QGraphicsTextItem* label = scene_->addText("Arduino Uno");
    label->setDefaultTextColor(QColor("#555577"));
    label->setFont(QFont("Courier New", 9));
    label->setPos(BOARD_X + BOARD_W/2 - 40, BOARD_Y + 10);

    // Chip label
    QGraphicsRectItem* chip = scene_->addRect(
        BOARD_X + 40, BOARD_Y + 80, 120, 60,
        QPen(QColor("#2a2a4c"), 1),
        QBrush(QColor("#0d1117"))
    );
    QGraphicsTextItem* chipLabel = scene_->addText("ATmega328P");
    chipLabel->setDefaultTextColor(QColor("#333355"));
    chipLabel->setFont(QFont("Courier New", 8));
    chipLabel->setPos(BOARD_X + 48, BOARD_Y + 100);

    // Draw pin dots along the right edge (digital pins 0-13)
    for (int i = 0; i <= 13; i++) {
        QPointF pos = pinLocation(i);
        scene_->addEllipse(
            pos.x() - 3, pos.y() - 3, 6, 6,
            QPen(QColor("#444466"), 1),
            QBrush(QColor("#2a2a3a"))
        );
        // Pin number label
        QGraphicsTextItem* pinNum = scene_->addText(QString::number(i));
        pinNum->setDefaultTextColor(QColor("#333355"));
        pinNum->setFont(QFont("Courier New", 7));
        pinNum->setPos(pos.x() + 6, pos.y() - 8);
    }

    // Draw pin dots along left edge (analog pins A0-A5 = pins 14-19)
    for (int i = 14; i <= 19; i++) {
        QPointF pos = pinLocation(i);
        scene_->addEllipse(
            pos.x() - 3, pos.y() - 3, 6, 6,
            QPen(QColor("#444466"), 1),
            QBrush(QColor("#2a2a3a"))
        );
        QGraphicsTextItem* pinNum = scene_->addText(QString("A%1").arg(i - 14));
        pinNum->setDefaultTextColor(QColor("#333355"));
        pinNum->setFont(QFont("Courier New", 7));
        pinNum->setPos(pos.x() - 24, pos.y() - 8);
    }
}

// -------------------------------------------------------
// drawComponent() -- draws one component and its wire
// -------------------------------------------------------
void CanvasWidget::drawComponent(
    const DetectedComponent& comp, int index, int total)
{
    // Lay out components on the right side for OUTPUT pins,
    // left side for INPUT pins
    bool is_output = (comp.type == ComponentType::LED      ||
                      comp.type == ComponentType::Buzzer   ||
                      comp.type == ComponentType::Servo    ||
                      comp.type == ComponentType::GenericOutput);

    int comp_w = 100;
    int comp_h = 44;

    QPointF pin_pos = pinLocation(comp.pin);

    // Space components vertically with even spacing
    float spacing = (float)BOARD_H / (total + 1);
    float comp_y  = BOARD_Y + spacing * (index + 1) - comp_h / 2.0f;
    float comp_x  = is_output
                    ? BOARD_X + BOARD_W + 80        // right side
                    : BOARD_X - comp_w - 80;        // left side

    // Component box -- created at local origin so child text items position correctly
    QGraphicsRectItem* rect = new QGraphicsRectItem(0, 0, comp_w, comp_h);
    rect->setPen(QPen(componentColor(comp.type, false).darker(150), 1));
    rect->setBrush(QBrush(componentColor(comp.type, false)));
    rect->setPos(comp_x, comp_y);
    scene_->addItem(rect);

    // Store component type in item data for updatePin()
    rect->setData(0, static_cast<int>(comp.type));
    pinItems_[comp.pin] = rect;

    pinTypes_[comp.pin] = comp.type;

    if (comp.type == ComponentType::Button) {
        buttonStates_[comp.pin] = false;
        rect->setFlag(QGraphicsItem::ItemIsSelectable);
        rect->setAcceptHoverEvents(true);
        rect->setCursor(Qt::PointingHandCursor);
        rect->setToolTip(QString("Click to press, release to let go"));
    }

    // Component label
    // Component label -- child of rect so clicks on text find the rect
    QGraphicsTextItem* typeText = new QGraphicsTextItem(rect);
    typeText->setPlainText(QString::fromStdString(comp.label));
    typeText->setDefaultTextColor(QColor("#cccccc"));
    typeText->setFont(QFont("Courier New", 8));
    typeText->setPos(6, 6);  // relative to rect

    // Pin name label
    if (!comp.pin_name.empty()) {
        QGraphicsTextItem* nameText = new QGraphicsTextItem(rect);
        nameText->setPlainText(QString::fromStdString(comp.pin_name));
        nameText->setDefaultTextColor(QColor("#888888"));
        nameText->setFont(QFont("Courier New", 7));
        nameText->setPos(6, 24);  // relative to rect
    }
    // Wire from component edge to pin on board
    QPointF wire_start = is_output
        ? QPointF(comp_x, comp_y + comp_h / 2.0)
        : QPointF(comp_x + comp_w, comp_y + comp_h / 2.0);

    // Route wire: horizontal then vertical to pin
    QPointF mid = QPointF(pin_pos.x(), wire_start.y());
    drawWire(wire_start, mid);
    drawWire(mid, pin_pos);
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    QGraphicsItem* item = itemAt(event->pos());

    // Walk up parent chain -- click may land on text label on top of rect
    while (item && !dynamic_cast<QGraphicsRectItem*>(item))
        item = item->parentItem();

    if (!item) { QGraphicsView::mousePressEvent(event); return; }

    int pin = pinItems_.key(
        dynamic_cast<QGraphicsRectItem*>(item), -1);

    if (pin >= 0 && pinTypes_.value(pin) == ComponentType::Button) {
        buttonStates_[pin] = true;
        dynamic_cast<QGraphicsRectItem*>(item)->setBrush(
            QBrush(componentColor(ComponentType::Button, true)));
        emit buttonPressed(pin, 0);
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    for (auto pin : buttonStates_.keys()) {
        if (buttonStates_[pin]) {
            buttonStates_[pin] = false;
            pinItems_[pin]->setBrush(
                QBrush(componentColor(ComponentType::Button, false)));
            emit buttonPressed(pin, 1); // HIGH = released for INPUT_PULLUP
        }
    }
    QGraphicsView::mouseReleaseEvent(event);
}


// -------------------------------------------------------
// drawWire()
// -------------------------------------------------------
void CanvasWidget::drawWire(QPointF from, QPointF to) {
    scene_->addLine(
        from.x(), from.y(), to.x(), to.y(),
        QPen(QColor("#444466"), 1, Qt::SolidLine, Qt::RoundCap)
    );
}

// -------------------------------------------------------
// pinLocation() -- returns the scene position of a pin
// on the board edge
// -------------------------------------------------------
QPointF CanvasWidget::pinLocation(int pin) {
    if (pin >= 0 && pin <= 13) {
        // Digital pins on right edge, evenly spaced
        float spacing = (float)BOARD_H / 15.0f;
        float y = BOARD_Y + spacing * (pin + 1);
        return QPointF(BOARD_X + BOARD_W, y);
    }
    if (pin >= 14 && pin <= 19) {
        // Analog pins on left edge
        float spacing = (float)BOARD_H / 7.0f;
        float y = BOARD_Y + spacing * (pin - 13);
        return QPointF(BOARD_X, y);
    }
    return QPointF(BOARD_X + BOARD_W / 2, BOARD_Y);
}

// -------------------------------------------------------
// componentColor() -- returns fill color for a component
// -------------------------------------------------------
QColor CanvasWidget::componentColor(ComponentType type, bool active) {
    static const std::map<ComponentType, QColor> activeColors = {
        { ComponentType::LED,           QColor("#ffdd44") },
        { ComponentType::Buzzer,        QColor("#ff8844") },
        { ComponentType::Servo,         QColor("#44aaff") },
        { ComponentType::Button,        QColor("#44ff88") },
    };

    static const std::map<ComponentType, QColor> inactiveColors = {
        { ComponentType::LED,           QColor("#3a3000") },
        { ComponentType::Button,        QColor("#003a15") },
        { ComponentType::Buzzer,        QColor("#3a1a00") },
        { ComponentType::Servo,         QColor("#001a3a") },
        { ComponentType::Potentiometer, QColor("#1a1a3a") },
        { ComponentType::LCD,           QColor("#001a1a") },
    };

    const auto& colors = active ? activeColors : inactiveColors;
    auto it = colors.find(type);
    return it != colors.end() ? it->second : QColor(active ? "#aaaaaa" : "#2a2a2a");
}