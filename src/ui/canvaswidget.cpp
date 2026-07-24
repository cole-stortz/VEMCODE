#include "src/ui/canvaswidget.h"
#include "src/core/circuit/componentregistry.h"
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QMouseEvent>
#include <QTransform>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <map>

// Chip/pin/component chrome -- static regardless of app theme, same as every
// component's own body/fill colors (LED_ACTIVE, wire_color, etc. in
// src/components/*.cpp). The board itself is the one exception (below) --
// its dark navy doesn't read against a light canvas viewport.
static const QColor COLOR_CHIP_BG        ("#3c3c62");
static const QColor COLOR_CHIP_BORDER    ("#5a5a89");
static const QColor COLOR_CHIP_LABEL     ("#9595d2");
static const QColor COLOR_PIN_DOT_BG     ("#2a2a3a");
static const QColor COLOR_PIN_DOT_BORDER ("#444466");
static const QColor COLOR_PIN_LABEL      ("#333355");
static const QColor COLOR_COMPONENT_SUBLABEL ("#888888");

// Board rectangle + label -- follows the app-wide theme.
static const QColor BOARD_BG_DARK      ("#1a1a2e");
static const QColor BOARD_BORDER_DARK  ("#3a3a5c");
static const QColor BOARD_LABEL_DARK   ("#555577");
static const QColor BOARD_BG_LIGHT     ("#d8d8e8");
static const QColor BOARD_BORDER_LIGHT ("#9898b8");
static const QColor BOARD_LABEL_LIGHT  ("#5c5c86");

// Viewport background (the empty area outside the board) is the one thing
// here that follows the app-wide theme -- everything else on the canvas
// stays fixed.
static const QColor VIEWPORT_BG_DARK  ("#1a1a1a");
static const QColor VIEWPORT_BG_LIGHT ("#dcdce2");

CanvasWidget::CanvasWidget(QWidget* parent)
    : QGraphicsView(parent)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    applyThemeStyle();
    drawBoard();
}

void CanvasWidget::applyThemeStyle() {
    const QColor& bg = darkTheme_ ? VIEWPORT_BG_DARK : VIEWPORT_BG_LIGHT;
    setStyleSheet(QString("background: %1; border: none;").arg(bg.name()));
}

void CanvasWidget::setDarkTheme(bool dark) {
    if (darkTheme_ == dark) return;
    darkTheme_ = dark;
    applyThemeStyle();
    refresh(lastComponents_); // redraws the board with the new theme's colors
}

void CanvasWidget::setLayoutMode(bool on) {
    layoutMode_ = on;
    setCursor(on ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (layoutMode_ && event->button() == Qt::LeftButton) {
        QGraphicsItem* hit = itemAt(event->pos());
        ComponentItem* comp = nullptr;
        while (hit) {
            comp = dynamic_cast<ComponentItem*>(hit);
            if (comp) break;
            hit = hit->parentItem();
        }
        if (comp) {
            draggedItem_ = comp;
            dragOffset_ = comp->pos() - mapToScene(event->pos());
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (draggedItem_) {
        draggedItem_->setPos(mapToScene(event->pos()) + dragOffset_);
        updateWires(draggedItem_);
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (draggedItem_) {
        auto it = componentInfo_.find(draggedItem_);
        if (it != componentInfo_.end())
            manualPositions_[it->primary_pin] = draggedItem_->pos();
        draggedItem_ = nullptr;
        setCursor(layoutMode_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasWidget::setZoom(qreal zoom) {
    zoomLevel_ = std::clamp(zoom, ZOOM_MIN, ZOOM_MAX);
    setTransform(QTransform::fromScale(zoomLevel_, zoomLevel_));
}

void CanvasWidget::zoomIn()  { setZoom(zoomLevel_ * 1.15); }
void CanvasWidget::zoomOut() { setZoom(zoomLevel_ / 1.15); }

void CanvasWidget::resetLayout() {
    manualPositions_.clear();
    refresh(lastComponents_);
}

void CanvasWidget::saveLayout(const QString& sketchPath) const {
    if (sketchPath.isEmpty()) return;

    QJsonObject positions;
    for (auto it = manualPositions_.constBegin(); it != manualPositions_.constEnd(); ++it) {
        QJsonObject pos;
        pos["x"] = it.value().x();
        pos["y"] = it.value().y();
        positions[QString::number(it.key())] = pos;
    }

    QJsonObject root;
    root["zoom"] = zoomLevel_;
    root["positions"] = positions;

    QFile file(sketchPath + ".vblayout");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void CanvasWidget::loadLayout(const QString& sketchPath) {
    manualPositions_.clear();
    setZoom(1.0);
    if (sketchPath.isEmpty()) return;

    QFile file(sketchPath + ".vblayout");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;
    QJsonObject root = doc.object();

    setZoom(root.value("zoom").toDouble(1.0));

    QJsonObject positions = root.value("positions").toObject();
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        bool ok = false;
        int pin = it.key().toInt(&ok);
        if (!ok) continue;
        QJsonObject pos = it.value().toObject();
        manualPositions_[pin] = QPointF(pos.value("x").toDouble(), pos.value("y").toDouble());
    }
}

void CanvasWidget::refresh(const std::vector<DetectedComponent>& components) {
    lastComponents_ = components;
    scene_->clear();
    pinItems_.clear();
    componentInfo_.clear();
    draggedItem_ = nullptr; // about to be deleted by scene_->clear()

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
    std::vector<Placement> manualPlacements; // user-dragged -- placed as-is, no auto-stacking
    for (const auto& comp : components) {
        if (comp.pin < 0) continue;
        const ComponentDefinition* def = ComponentRegistry::instance().find_by_type(comp.type_name);
        if (!def) continue;

        ComponentItem* item = def->create_item(comp.pin, nullptr);
        // Must happen before boundingRect() below -- device count changes
        // the item's rendered size, and this is the only phase where
        // boundingRect() gets queried for layout math.
        item->configureDeviceCount(comp.num_devices);
        QRectF box = item->boundingRect();
        int comp_w = (int)box.width();
        int comp_h = (int)box.height();

        auto manual = manualPositions_.find(comp.pin);
        if (manual != manualPositions_.end()) {
            manualPlacements.push_back({&comp, def, item, (float)manual->x(), comp_w, comp_h, (float)manual->y()});
            continue;
        }

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

    for (auto& p : manualPlacements)
        placeComponent(*p.comp, p.def, p.item, p.comp_x, p.target_y, p.comp_w, p.comp_h);
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

void CanvasWidget::updateMatrixRow(int pin, int addr, int row, int bits) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;
    it.value()->updateMatrixRow(addr, row, bits);
}

void CanvasWidget::onComponentInput(int pin, int eventType, QVariant value) {
    emit inputChanged(pin, eventType, value);
}

void CanvasWidget::drawBoard() {
    const QColor& board_bg     = darkTheme_ ? BOARD_BG_DARK     : BOARD_BG_LIGHT;
    const QColor& board_border = darkTheme_ ? BOARD_BORDER_DARK : BOARD_BORDER_LIGHT;
    const QColor& board_label  = darkTheme_ ? BOARD_LABEL_DARK  : BOARD_LABEL_LIGHT;

    scene_->addRect(
        BOARD_X, BOARD_Y, BOARD_W, BOARD_H,
        QPen(board_border, 2),
        QBrush(board_bg)
    );

    QGraphicsTextItem* label = scene_->addText(profile_.name);
    label->setDefaultTextColor(board_label);
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
        if (comp.rows > 0 && comp.cols > 0)
            item->configureRowsCols(comp.rows, comp.cols);
        item->configureMultiPin(comp.pins);
    }
    item->emitInitialValue();

    
    QGraphicsTextItem* typeText = new QGraphicsTextItem(item);
    typeText->setPlainText(QString::fromStdString(comp.label));
    typeText->setDefaultTextColor(COLOR_COMPONENT_SUBLABEL);
    typeText->setFont(QFont("Courier New", 8));
    typeText->setPos(6, -16);

    // Removed temporarily to remove bloat on the canvas
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

    componentInfo_[item] = ComponentInfo{ comp.pin, is_output, wire_pins, def->wire_color, {} };
    updateWires(item);
}

QGraphicsLineItem* CanvasWidget::drawWire(QPointF from, QPointF to, const QColor& color) {
    return scene_->addLine(
        from.x(), from.y(), to.x(), to.y(),
        QPen(color, 1, Qt::SolidLine, Qt::RoundCap)
    );
}

// Re-derives every wire segment for one component from its current scene
// position -- used both for the initial placement and to keep wires attached
// while the component is being dragged in layout mode.
void CanvasWidget::updateWires(ComponentItem* item) {
    auto it = componentInfo_.find(item);
    if (it == componentInfo_.end()) return;
    ComponentInfo& info = it.value();

    for (QGraphicsLineItem* line : info.wire_lines)
        delete line;
    info.wire_lines.clear();

    float comp_x = item->x();
    float comp_y = item->y();
    int comp_w = (int)item->boundingRect().width();

    int i = 0;
    for (int wpin : info.wire_pins) {
        if (wpin < 0) { i++; continue; }

        QPointF target = pinLocation(wpin);

        // Each wire attaches at a different y so they don't stack
        float attach_y = comp_y + 15.0f + i * WIRE_SPACING;

        QPointF comp_edge = info.is_output
            ? QPointF(comp_x, attach_y)
            : QPointF(comp_x + comp_w, attach_y);

        float inter_x;
        if (isAnalogPin(wpin)) {
            // Turn near the pin (in the gap before the digital column), not
            // near the destination -- otherwise the vertical segment cuts
            // through whatever got stacked between the pin and its component.
            inter_x = BOARD_X - 20.0f - i * WIRE_SPACING;
        } else if (info.is_output) {
            // Digital output: component sits on the same side as the pin, so
            // there's no column to cross -- turning near the component is fine.
            inter_x = comp_x - 10.0f - i * WIRE_SPACING;
        } else {
            // Digital input: same fix as analog, mirrored -- turn right next
            // to the pin (board's right edge) instead of next to the component.
            inter_x = target.x() + 10.0f + i * WIRE_SPACING;
        }
        QPointF mid1(inter_x, target.y());
        QPointF mid2(inter_x, attach_y);
        info.wire_lines.push_back(drawWire(target, mid1, info.wire_color));
        info.wire_lines.push_back(drawWire(mid1, mid2, info.wire_color));
        info.wire_lines.push_back(drawWire(mid2, comp_edge, info.wire_color));
        i++;
    }
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