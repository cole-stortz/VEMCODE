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

// -------------------------------------------------------
// Canvas color palette -- change these to retheme the canvas
// -------------------------------------------------------

// Board colors
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

// Component active colors (when pin is HIGH or button is pressed)
static const QColor COLOR_LED_ACTIVE     ("#ffdd44");
static const QColor COLOR_BUTTON_ACTIVE  ("#44ff88");
static const QColor COLOR_BUZZER_ACTIVE  ("#ff8844");
static const QColor COLOR_SERVO_ACTIVE   ("#44aaff");
static const QColor COLOR_GENERIC_ACTIVE ("#aaaaaa");

// Component inactive colors (default state)
static const QColor COLOR_LED_INACTIVE        ("#3a3000");
static const QColor COLOR_BUTTON_INACTIVE     ("#003a15");
static const QColor COLOR_BUZZER_INACTIVE     ("#3a1a00");
static const QColor COLOR_SERVO_INACTIVE      ("#001a3a");
static const QColor COLOR_POT_INACTIVE        ("#1a1a3a");
static const QColor COLOR_LCD_INACTIVE        ("#001a1a");
static const QColor COLOR_GENERIC_INACTIVE    ("#2a2a2a");

// Component text colors
static const QColor COLOR_COMPONENT_LABEL     ("#cccccc");
static const QColor COLOR_COMPONENT_SUBLABEL  ("#888888");

// -------------------------------------------------------

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

void CanvasWidget::updatePin(int pin, int value) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;

    // Find the component type for this pin to get the right active color
    // We store it in the item's data slot
    ComponentType type = static_cast<ComponentType>(it.value()->data(0).toInt());
    it.value()->setBrush(QBrush(componentColor(type, value == 1)));
}

void CanvasWidget::drawBoard() {
    // Board body
    scene_->addRect(
        BOARD_X, BOARD_Y, BOARD_W, BOARD_H,
        QPen(COLOR_BOARD_BORDER, 2),
        QBrush(COLOR_BOARD_BG)
    );

    // Board label
    QGraphicsTextItem* label = scene_->addText("Arduino Uno");
    label->setDefaultTextColor(COLOR_BOARD_LABEL);
    label->setFont(QFont("Courier New", 9));
    label->setPos(BOARD_X + BOARD_W/2 - 40, BOARD_Y + 10);

    // Chip
    scene_->addRect(
        BOARD_X + 40, BOARD_Y + 80, 120, 60,
        QPen(COLOR_CHIP_BORDER, 1),
        QBrush(COLOR_CHIP_BG)
    );
    QGraphicsTextItem* chipLabel = scene_->addText("ATmega328P");
    chipLabel->setDefaultTextColor(COLOR_CHIP_LABEL);
    chipLabel->setFont(QFont("Courier New", 8));
    chipLabel->setPos(BOARD_X + 48, BOARD_Y + 100);

    // Digital pins on right edge (0-13)
    for (int i = 0; i <= 13; i++) {
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

    // Analog pins on left edge (A0-A5 = pins 14-19)
    for (int i = 14; i <= 19; i++) {
        QPointF pos = pinLocation(i);
        scene_->addEllipse(
            pos.x() - 3, pos.y() - 3, 6, 6,
            QPen(COLOR_PIN_DOT_BORDER, 1),
            QBrush(COLOR_PIN_DOT_BG)
        );
        QGraphicsTextItem* pinNum = scene_->addText(QString("A%1").arg(i - 14));
        pinNum->setDefaultTextColor(COLOR_PIN_LABEL);
        pinNum->setFont(QFont("Courier New", 7));
        pinNum->setPos(pos.x() - 24, pos.y() - 8);
    }
}

void CanvasWidget::drawComponent(
    const DetectedComponent& comp, int index, int total)
{
    // Output components go on right side, input on left
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
                    ? BOARD_X + BOARD_W + 80
                    : BOARD_X - comp_w - 80;

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

    // Component label -- child of rect so clicks on text find the rect
    QGraphicsTextItem* typeText = new QGraphicsTextItem(rect);
    typeText->setPlainText(QString::fromStdString(comp.label));
    typeText->setDefaultTextColor(COLOR_COMPONENT_LABEL);
    typeText->setFont(QFont("Courier New", 8));
    typeText->setPos(6, 6);

    // Pin name label (e.g. LED_PIN)
    if (!comp.pin_name.empty()) {
        QGraphicsTextItem* nameText = new QGraphicsTextItem(rect);
        nameText->setPlainText(QString::fromStdString(comp.pin_name));
        nameText->setDefaultTextColor(COLOR_COMPONENT_SUBLABEL);
        nameText->setFont(QFont("Courier New", 7));
        nameText->setPos(6, 24);
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
        emit buttonPressed(pin, 0); // LOW = pressed for INPUT_PULLUP
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

void CanvasWidget::drawWire(QPointF from, QPointF to) {
    scene_->addLine(
        from.x(), from.y(), to.x(), to.y(),
        QPen(COLOR_WIRE, 1, Qt::SolidLine, Qt::RoundCap)
    );
}

QPointF CanvasWidget::pinLocation(int pin) {
    if (pin >= 0 && pin <= 13) {
        float spacing = (float)BOARD_H / 15.0f;
        float y = BOARD_Y + spacing * (pin + 1);
        return QPointF(BOARD_X + BOARD_W, y);
    }
    if (pin >= 14 && pin <= 19) {
        float spacing = (float)BOARD_H / 7.0f;
        float y = BOARD_Y + spacing * (pin - 13);
        return QPointF(BOARD_X, y);
    }
    return QPointF(BOARD_X + BOARD_W / 2, BOARD_Y);
}

QColor CanvasWidget::componentColor(ComponentType type, bool active) {
    static const std::map<ComponentType, QColor> activeColors = {
        { ComponentType::LED,           COLOR_LED_ACTIVE     },
        { ComponentType::Button,        COLOR_BUTTON_ACTIVE  },
        { ComponentType::Buzzer,        COLOR_BUZZER_ACTIVE  },
        { ComponentType::Servo,         COLOR_SERVO_ACTIVE   },
    };

    static const std::map<ComponentType, QColor> inactiveColors = {
        { ComponentType::LED,           COLOR_LED_INACTIVE   },
        { ComponentType::Button,        COLOR_BUTTON_INACTIVE},
        { ComponentType::Buzzer,        COLOR_BUZZER_INACTIVE},
        { ComponentType::Servo,         COLOR_SERVO_INACTIVE },
        { ComponentType::Potentiometer, COLOR_POT_INACTIVE   },
        { ComponentType::LCD,           COLOR_LCD_INACTIVE   },
    };

    const auto& colors = active ? activeColors : inactiveColors;
    auto it = colors.find(type);
    return it != colors.end() ? it->second
                              : (active ? COLOR_GENERIC_ACTIVE : COLOR_GENERIC_INACTIVE);
}