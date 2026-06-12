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
static const QColor COLOR_SWITCH_ACTIVE      ("#44ff88");
static const QColor COLOR_POT_ACTIVE         ("#44ffcc");
static const QColor COLOR_LIGHT_ACTIVE       ("#ffff44");
static const QColor COLOR_TEMP_ACTIVE        ("#ff6644");
static const QColor COLOR_ANALOG_ACTIVE      ("#aaaaaa");
static const QColor COLOR_LCD_ACTIVE          ("#3d0076");
static const QColor COLOR_DISTANCE_ACTIVE     ("#44ffff");
static const QColor COLOR_HBRIDGE_ACTIVE      ("#ff44aa");
static const QColor COLOR_COLORSENSOR_ACTIVE ("#aa44ff");

// Component inactive colors (default state)
static const QColor COLOR_LED_INACTIVE        ("#3a3000");
static const QColor COLOR_BUTTON_INACTIVE     ("#003a15");
static const QColor COLOR_BUZZER_INACTIVE     ("#3a1a00");
static const QColor COLOR_SERVO_INACTIVE      ("#001a3a");
static const QColor COLOR_POT_INACTIVE        ("#1a1a3a");
static const QColor COLOR_LCD_INACTIVE        ("#0b0213");
static const QColor COLOR_GENERIC_INACTIVE    ("#2a2a2a");
static const QColor COLOR_SWITCH_INACTIVE    ("#003a15");
static const QColor COLOR_LIGHT_INACTIVE     ("#3a3a00");
static const QColor COLOR_TEMP_INACTIVE      ("#3a1500");
static const QColor COLOR_ANALOG_INACTIVE    ("#2a2a2a");
static const QColor COLOR_DISTANCE_INACTIVE  ("#003a3a");
static const QColor COLOR_HBRIDGE_INACTIVE   ("#3a0020");
static const QColor COLOR_COLORSENSOR_INACTIVE ("#1a0040");

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
    switchStates_.clear();
    buttonStates_.clear();
    analogValues_.clear();
    servoLabels_.clear();
    lcdRow0Labels_.clear();
    lcdRow1Labels_.clear();
    dragPin_ = -1;

    drawBoard();

    for (const auto& comp : components) {
        if (comp.pin >= 0)
            drawComponent(comp);
    }
}

void CanvasWidget::updatePin(int pin, int value) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;

    ComponentType type = static_cast<ComponentType>(it.value()->data(0).toInt());

    if (type == ComponentType::HBridgeMotor) {
        auto rep_it = motorPinToRep_.find(pin);
        if (rep_it == motorPinToRep_.end()) return;
        int rep = rep_it.value();

        auto& state = motorStates_[rep];
        if (pin == rep)
            state.pwm = value;
        else if (pin == motorCwisePin_.value(rep, -1))
            state.cwise = value;
        else if (pin == motorAntiCwisePin_.value(rep, -1))
            state.anti_cwise = value;

        bool active = (state.cwise || state.anti_cwise) && state.pwm > 0;
        it.value()->setBrush(QBrush(componentColor(type, active)));

        QString dir;
        if      (state.cwise && state.anti_cwise) dir = "BRAKE";
        else if (state.cwise)                     dir = "CW";
        else if (state.anti_cwise)                dir = "CCW";
        else                                      dir = "STOP";

        auto label_it = motorLabels_.find(rep);
        if (label_it != motorLabels_.end())
            label_it.value()->setPlainText(dir + "\nPWM: " + QString::number(state.pwm));
        return;
    }

    it.value()->setBrush(QBrush(componentColor(type, value > 0)));

    if (type == ComponentType::Servo) {
        int angle = value * 180 / profile_.pwm_resolution;
        auto label_it = servoLabels_.find(pin);
        if (label_it != servoLabels_.end())
            label_it.value()->setPlainText(QString::number(angle) + "°");
    }
    if (type == ComponentType::Buzzer) {
        auto label_it = servoLabels_.find(pin);
        if (label_it != servoLabels_.end())
            label_it.value()->setPlainText("PWM: " + QString::number(value));
    }
}

void CanvasWidget::updateLcdText(int pin, int row, const QString& text) {
    auto& map = (row == 0) ? lcdRow0Labels_ : lcdRow1Labels_;
    auto it = map.find(pin);
    if (it == map.end()) return;
    it.value()->setPlainText(text.left(16).leftJustified(16));
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

void CanvasWidget::drawComponent(const DetectedComponent& comp)
{
    // Output components go on right side, input on left
    bool is_output = (comp.type == ComponentType::LED          ||
                      comp.type == ComponentType::Buzzer       ||
                      comp.type == ComponentType::Servo        ||
                      comp.type == ComponentType::HBridgeMotor ||
                      comp.type == ComponentType::LCD          ||
                      comp.type == ComponentType::GenericOutput);


    int comp_w = 100;
    int comp_h = (comp.type == ComponentType::ColorSensor)  ? 64
               : (comp.type == ComponentType::HBridgeMotor) ? 54
               : (comp.type == ComponentType::LCD)          ? 54
               : 44;

    QPointF pin_pos = pinLocation(comp.pin);

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
    // Output components use even spacing, inputs align with their pin
    float comp_y = pin_pos.y() - comp_h / 2.0f;

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

    // H-bridge motor: register all 3 pins and create state/label tracking
    if (comp.type == ComponentType::HBridgeMotor && comp.pins.size() == 3) {
        int rep        = comp.pin;        // pins[0] = PWM = rep
        int cwise_pin  = comp.pins[1];
        int anti_pin   = comp.pins[2];
        motorStates_[rep]       = MotorState{};
        motorCwisePin_[rep]     = cwise_pin;
        motorAntiCwisePin_[rep] = anti_pin;
        for (int mp : comp.pins) {
            if (mp >= 0) {
                pinItems_[mp]      = rect;
                pinTypes_[mp]      = comp.type;
                motorPinToRep_[mp] = rep;
            }
        }
        QGraphicsTextItem* motorLabel = new QGraphicsTextItem("STOP\nPWM: 0", rect);
        motorLabel->setDefaultTextColor(COLOR_COMPONENT_LABEL);
        motorLabel->setFont(QFont("Courier New", 8));
        motorLabel->setPos(6, 20);
        motorLabels_[rep] = motorLabel;
    }

    // Button -- click/release injects LOW/HIGH (with bounce simulation)
    if (comp.type == ComponentType::Button) {
        buttonStates_[comp.pin] = false;
        rect->setFlag(QGraphicsItem::ItemIsSelectable);
        rect->setAcceptHoverEvents(true);
        rect->setCursor(Qt::PointingHandCursor);
        rect->setToolTip("Click to press, release to let go");
    }

    // ButtonClean -- same interaction as Button but no bounce simulation
    if (comp.type == ComponentType::ButtonClean) {
        buttonStates_[comp.pin] = false;
        rect->setFlag(QGraphicsItem::ItemIsSelectable);
        rect->setAcceptHoverEvents(true);
        rect->setCursor(Qt::PointingHandCursor);
        rect->setToolTip("Click to press, release to let go (ideal — no bounce)");
    }

    // Switch -- click toggles state
    if (comp.type == ComponentType::Switch) {
        buttonStates_[comp.pin] = false;
        rect->setFlag(QGraphicsItem::ItemIsSelectable);
        rect->setAcceptHoverEvents(true);
        rect->setCursor(Qt::PointingHandCursor);
        rect->setToolTip("Click to toggle");
    }

    // Potentiometer -- drag to set analog value
    if (comp.type == ComponentType::Potentiometer) {
        rect->setToolTip("Drag to set analog value (0-1023)");
    }

    // Sensor types -- show current analog value
    if (comp.type == ComponentType::LightSensor  ||
        comp.type == ComponentType::TempSensor   ||
        comp.type == ComponentType::AnalogSensor) {
        rect->setToolTip("Analog input — value: 0");
    }

    if (comp.type == ComponentType::DistanceSensor) {
        int echo_pin = comp.pin;
        QLineEdit* input = new QLineEdit();
        input->setFixedSize(60, 20);
        input->setPlaceholderText("cm");
        input->setStyleSheet("background:#001a1a; color:#44ffff; border:1px solid #44ffff;");
        QGraphicsProxyWidget* proxy = scene_->addWidget(input);
        proxy->setParentItem(rect);
        proxy->setPos(comp_w - 66, comp_h - 26);
        connect(input, &QLineEdit::textChanged, this, [this, echo_pin](const QString& text) {
            bool ok;
            float cm = text.toFloat(&ok);
            if (ok && cm >= 0)
                emit pulseInjected(echo_pin, (unsigned long)std::ceil(cm * 2.0f / 0.034f));
        });
        input->setText("10");
    }

    if (comp.type == ComponentType::LightSensor  ||
        comp.type == ComponentType::TempSensor   ||
        comp.type == ComponentType::AnalogSensor) {
        int sensor_pin = comp.pin;
        QLineEdit* input = new QLineEdit("0");
        input->setFixedSize(60, 20);
        input->setPlaceholderText("0-1023");
        input->setStyleSheet(
            "background:#1a1a00; color:#ffff44; border:1px solid #ffff44;");
        QGraphicsProxyWidget* proxy = scene_->addWidget(input);
        proxy->setParentItem(rect);
        proxy->setPos(comp_w - 66, comp_h - 26);
        connect(input, &QLineEdit::textChanged, this, [this, sensor_pin](const QString& text) {
            bool ok;
            int val = text.toInt(&ok);
            if (ok)
                emit analogInjected(sensor_pin, qBound(0, val, 1023));
        });
    }

    if (comp.type == ComponentType::ColorSensor && comp.pins.size() >= 5) {
        int s2_pin  = comp.pins[2];
        int s3_pin  = comp.pins[3];
        int out_pin = comp.pins[4];

        auto make_input = [&](const char* fg, const char* bg, int x) -> QLineEdit* {
            QLineEdit* in = new QLineEdit("0");
            in->setFixedSize(26, 16);
            in->setStyleSheet(QString("background:%1; color:%2; border:1px solid %2;")
                              .arg(bg).arg(fg));
            QGraphicsProxyWidget* proxy = scene_->addWidget(in);
            proxy->setParentItem(rect);
            proxy->setPos(x, comp_h - 22);
            return in;
        };

        QLineEdit* r_in = make_input("#ff4444", "#1a0000", 4);
        QLineEdit* g_in = make_input("#44ff44", "#001a00", 34);
        QLineEdit* b_in = make_input("#4444ff", "#00001a", 64);

        auto emit_color = [this, r_in, g_in, b_in, out_pin, s2_pin, s3_pin]() {
            bool rok, gok, bok;
            int r = qBound(0, r_in->text().toInt(&rok), 255);
            int g = qBound(0, g_in->text().toInt(&gok), 255);
            int b = qBound(0, b_in->text().toInt(&bok), 255);
            if (rok && gok && bok)
                emit colorInjected(out_pin, s2_pin, s3_pin, r, g, b);
        };

        connect(r_in, &QLineEdit::textChanged, this, emit_color);
        connect(g_in, &QLineEdit::textChanged, this, emit_color);
        connect(b_in, &QLineEdit::textChanged, this, emit_color);
        emit_color();
    }

    if (comp.type == ComponentType::Servo) {
        QGraphicsTextItem* angleText = new QGraphicsTextItem("0°", rect);
        angleText->setDefaultTextColor(COLOR_COMPONENT_LABEL);
        angleText->setFont(QFont("Courier New", 9));
        angleText->setPos(comp_w - 36, 15);
        servoLabels_[comp.pin] = angleText;
    }

    if (comp.type == ComponentType::LCD) {
        QGraphicsTextItem* row0 = new QGraphicsTextItem("                ", rect);
        row0->setDefaultTextColor(COLOR_COMPONENT_LABEL);
        row0->setFont(QFont("Courier New", 7));
        row0->setPos(6, 22);
        lcdRow0Labels_[comp.pin] = row0;

        QGraphicsTextItem* row1 = new QGraphicsTextItem("                ", rect);
        row1->setDefaultTextColor(COLOR_COMPONENT_LABEL);
        row1->setFont(QFont("Courier New", 7));
        row1->setPos(6, 36);
        lcdRow1Labels_[comp.pin] = row1;
    }
    if (comp.type == ComponentType::Buzzer) {
        // Display current PWM value for buzzer
        QGraphicsTextItem* pwmText = new QGraphicsTextItem("PWM: 0", rect);
        pwmText->setDefaultTextColor(COLOR_COMPONENT_LABEL);
        pwmText->setFont(QFont("Courier New", 8));
        pwmText->setPos(6, 20);
        servoLabels_[comp.pin] = pwmText;
    }
    // Component label -- child of rect so clicks on text find the rect
    QGraphicsTextItem* typeText = new QGraphicsTextItem(rect);
    typeText->setPlainText(QString::fromStdString(comp.label));
    typeText->setDefaultTextColor(COLOR_COMPONENT_LABEL);
    typeText->setFont(QFont("Courier New", 8));
    typeText->setPos(6, 2);

    // Pin name sublabel, if available (e.g. "LED_PIN" or "Echo Pin")
    if (!comp.pin_name.empty()) {
        QGraphicsTextItem* nameText = new QGraphicsTextItem(rect);
        nameText->setPlainText(QString::fromStdString(comp.pin_name));
        nameText->setDefaultTextColor(COLOR_COMPONENT_SUBLABEL);
        nameText->setFont(QFont("Courier New", 7));
        nameText->setPos(6, -15);
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
        emit buttonBounced(pin, 0); // LOW = pressed; bounced path
        return;
    }

    if (pin >= 0 && pinTypes_.value(pin) == ComponentType::ButtonClean) {
        buttonStates_[pin] = true;
        dynamic_cast<QGraphicsRectItem*>(item)->setBrush(
            QBrush(componentColor(ComponentType::ButtonClean, true)));
        emit buttonPressed(pin, 0); // LOW = pressed; no bounce
        return;
    }

    // Switch -- toggle state on each click, stays until clicked again
    if (pin >= 0 && pinTypes_.value(pin) == ComponentType::Switch) {
        bool new_state = !switchStates_.value(pin, false);
        switchStates_[pin] = new_state;
        dynamic_cast<QGraphicsRectItem*>(item)->setBrush(
            QBrush(componentColor(ComponentType::Switch, new_state)));
        // HIGH when on, LOW when off (opposite of INPUT_PULLUP button)
        emit buttonPressed(pin, new_state ? 1 : 0);
        return;
    }

    if (pin >= 0 && pinTypes_.value(pin) == ComponentType::Potentiometer) {
        dragPin_        = pin;
        dragStartY_     = event->pos().y();
        dragStartValue_ = analogValues_.value(pin, 512);
        dynamic_cast<QGraphicsRectItem*>(item)->setCursor(Qt::SizeVerCursor);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (dragPin_ >= 0) {
        dragPin_ = -1;
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    for (auto pin : buttonStates_.keys()) {
        if (buttonStates_[pin]) {
            buttonStates_[pin] = false;
            ComponentType t = pinTypes_.value(pin);
            pinItems_[pin]->setBrush(QBrush(componentColor(t, false)));
            if (t == ComponentType::ButtonClean)
                emit buttonPressed(pin, 1);
            else
                emit buttonBounced(pin, 1); // bounced release
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

QColor CanvasWidget::componentColor(ComponentType type, bool active) {
    static const std::map<ComponentType, QColor> activeColors = {
        { ComponentType::LED,           COLOR_LED_ACTIVE     },
        { ComponentType::Button,        COLOR_BUTTON_ACTIVE  },
        { ComponentType::ButtonClean,   COLOR_BUTTON_ACTIVE  },
        { ComponentType::Switch,        COLOR_SWITCH_ACTIVE  },
        { ComponentType::Buzzer,        COLOR_BUZZER_ACTIVE  },
        { ComponentType::Servo,         COLOR_SERVO_ACTIVE   },
        { ComponentType::Potentiometer, COLOR_POT_ACTIVE     },
        { ComponentType::LightSensor,   COLOR_LIGHT_ACTIVE   },
        { ComponentType::TempSensor,    COLOR_TEMP_ACTIVE    },
        { ComponentType::AnalogSensor,  COLOR_ANALOG_ACTIVE  },
        { ComponentType::DistanceSensor,COLOR_DISTANCE_ACTIVE },
        { ComponentType::HBridgeMotor,  COLOR_HBRIDGE_ACTIVE },
        { ComponentType::ColorSensor,   COLOR_COLORSENSOR_ACTIVE },
        { ComponentType::LCD,            COLOR_LCD_ACTIVE  },
    };

    static const std::map<ComponentType, QColor> inactiveColors = {
        { ComponentType::LED,           COLOR_LED_INACTIVE   },
        { ComponentType::Button,        COLOR_BUTTON_INACTIVE},
        { ComponentType::ButtonClean,   COLOR_BUTTON_INACTIVE},
        { ComponentType::Switch,        COLOR_SWITCH_INACTIVE},
        { ComponentType::Buzzer,        COLOR_BUZZER_INACTIVE},
        { ComponentType::Servo,         COLOR_SERVO_INACTIVE },
        { ComponentType::Potentiometer, COLOR_POT_INACTIVE   },
        { ComponentType::LightSensor,   COLOR_LIGHT_INACTIVE },
        { ComponentType::TempSensor,    COLOR_TEMP_INACTIVE  },
        { ComponentType::AnalogSensor,  COLOR_ANALOG_INACTIVE},
        { ComponentType::DistanceSensor,COLOR_DISTANCE_INACTIVE },
        { ComponentType::HBridgeMotor,  COLOR_HBRIDGE_INACTIVE },
        { ComponentType::ColorSensor,   COLOR_COLORSENSOR_INACTIVE },
        { ComponentType::LCD,            COLOR_LCD_INACTIVE},
    };

    const auto& colors = active ? activeColors : inactiveColors;
    auto it = colors.find(type);
    return it != colors.end() ? it->second
                              : (active ? COLOR_GENERIC_ACTIVE : COLOR_GENERIC_INACTIVE);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragPin_ < 0) { QGraphicsView::mouseMoveEvent(event); return; }

    // Map vertical drag to 0-1023
    // Drag up = higher value, drag down = lower value
    int delta = dragStartY_ - event->pos().y();
    int new_value = qBound(0, dragStartValue_ + delta * 4, 1023);

    analogValues_[dragPin_] = new_value;

    auto it = pinItems_.find(dragPin_);
    if (it != pinItems_.end()) {
        float ratio = new_value / 1023.0f;
        QColor active = componentColor(ComponentType::Potentiometer, true);
        QColor inactive = componentColor(ComponentType::Potentiometer, false);
        int r = inactive.red()   + ratio * (active.red()   - inactive.red());
        int g = inactive.green() + ratio * (active.green() - inactive.green());
        int b = inactive.blue()  + ratio * (active.blue()  - inactive.blue());
        it.value()->setBrush(QBrush(QColor(r, g, b)));

        it.value()->setToolTip(
            QString("Value: %1 / 1023").arg(new_value));
    }

    emit potentiometerChanged(dragPin_, new_value);
}
