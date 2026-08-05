#include "src/ui/canvaswidget.h"
#include "src/core/circuit/componentregistry.h"
#include "src/core/circuit/i2cbus.h"
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QRadialGradient>
#include <QMouseEvent>
#include <QTransform>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QColorDialog>
#include <algorithm>
#include <map>
#include <qmessagebox.h>
#include <qnamespace.h>

// Component chrome -- static regardless of app theme, like every component's own body/fill colors.
static const QColor COLOR_COMPONENT_SUBLABEL ("#888888");

// Power indicator LED -- fixed colors regardless of board_color, same as a
// real board's power LED doesn't change with the PCB color.
static const QColor COLOR_POWER_LED_ON  ("#4ade80");
static const QColor COLOR_POWER_LED_OFF ("#3a3a3a");

// Mounting holes -- fixed colors, same reasoning as the power LED (real
// hardware, doesn't change with board_color).
static const QColor COLOR_MOUNT_HOLE         ("#111111");
static const QColor COLOR_MOUNT_HOLE_BORDER  ("#606068");

// Board rect, chip, and pin dot colors are all derived from a single base color (see drawBoard()), not fixed constants.

// Viewport background is the one thing here that follows the app-wide theme -- everything else stays fixed.
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

void CanvasWidget::setProfile(BoardProfile p) {
    profile_ = p;
    BOARD_H = p.pin_count * 14;
    BOARD_W = p.board_width;
    refresh(lastComponents_); // redraws the board at the new profile's dimensions
}

void CanvasWidget::setSketchRunning(bool running) {
    if (sketchRunning_ == running) return;
    sketchRunning_ = running;
    refresh(lastComponents_); // redraws the board with the power LED's new state
}

void CanvasWidget::setLayoutMode(bool on) {
    layoutMode_ = on;
    setCursor(on ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier)) {
        QGraphicsItem* originalHit = itemAt(event->pos());
        QGraphicsItem* hit = originalHit;
        ComponentItem* comp = nullptr;
        while (hit) {
            comp = dynamic_cast<ComponentItem*>(hit);
            if (comp) break;
            hit = hit->parentItem();
        }

        if (!comp && originalHit && originalHit == boardRectItem_) {
            QColor current = boardColorOverride_.isValid() ? boardColorOverride_ : profile_.board_color;
            QColor chosen = QColorDialog::getColor(current, this, "Board Color");
            if (chosen.isValid()) {
                boardColorOverride_ = chosen;
                refresh(lastComponents_);
            }
            event->accept();
            return;
        }

        if (comp && comp->supportsRotation()) {
            bool ok = false;
            QStringList options = {"0", "90", "180", "270"};
            QString choice = QInputDialog::getItem(
                this, "Rotation", "Degrees clockwise:", options, 0, false, &ok);
            if (ok) {
                int degrees = choice.toInt();
                comp->configureRotation(degrees);

                auto it = componentInfo_.find(comp);
                if (it != componentInfo_.end())
                    manualRotations_[it->primary_pin] = degrees;
            }
        }

        if (comp && comp->supportsColorConfig()) {
            QColor chosen = QColorDialog::getColor(comp->baseColor(), this, "Color: ");
            if (chosen.isValid()) {
                comp->configureColor(chosen);

                auto it = componentInfo_.find(comp);
                if (it != componentInfo_.end())
                    manualColors_[it->primary_pin] = chosen;
            }
        }

        if (comp && comp->supportsPolarity()) {
            QStringList options = {"Common Cathode", "Common Anode"};
            int current = comp->isCommonAnode() ? 1 : 0;
            bool ok = false;
            QString choice = QInputDialog::getItem(
                this, "Polarity", "Wiring type:", options, current, false, &ok);
            if (ok) {
                bool commonAnode = (choice == "Common Anode");
                comp->configurePolarity(commonAnode);

                auto it = componentInfo_.find(comp);
                if (it != componentInfo_.end())
                    manualPolarities_[it->primary_pin] = commonAnode;
            }
        }
        event->accept();
        return;
    }

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

        // Cascade: an I2C chain link's wire targets the device it's chained to (chain_prev), not a fixed board
        // pin, so dependents need re-routing too. Chains are short linked lists, so re-scanning to fixpoint is enough.
        ComponentItem* moved = draggedItem_;
        bool foundDependent = true;
        while (foundDependent) {
            foundDependent = false;
            for (auto it = componentInfo_.begin(); it != componentInfo_.end(); ++it) {
                if (it.value().chain_prev == moved) {
                    updateWires(it.key());
                    moved = it.key();
                    foundDependent = true;
                    break;
                }
            }
        }

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
    manualRotations_.clear();
    manualColors_.clear();
    manualPolarities_.clear();
    boardColorOverride_ = QColor();
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

    QJsonObject rotations;
    for (auto it = manualRotations_.constBegin(); it != manualRotations_.constEnd(); ++it) {
        rotations[QString::number(it.key())] = it.value();
    }

    QJsonObject colors;
    for (auto it = manualColors_.constBegin(); it != manualColors_.constEnd(); ++it) {
        colors[QString::number(it.key())] = it.value().name(QColor::HexArgb);
    }

    QJsonObject polarities;
    for (auto it = manualPolarities_.constBegin(); it != manualPolarities_.constEnd(); ++it) {
        polarities[QString::number(it.key())] = it.value();
    }

    QJsonObject root;
    root["zoom"] = zoomLevel_;
    root["positions"] = positions;
    root["rotations"] = rotations;
    root["colors"] = colors;
    root["polarities"] = polarities;
    if (boardColorOverride_.isValid())
        root["boardColor"] = boardColorOverride_.name(QColor::HexArgb);

    QFile file(sketchPath + ".vblayout");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void CanvasWidget::loadLayout(const QString& sketchPath) {
    manualPositions_.clear();
    manualRotations_.clear();
    manualColors_.clear();
    manualPolarities_.clear();
    boardColorOverride_ = QColor();
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

    QJsonObject rotations = root.value("rotations").toObject();
    for (auto it = rotations.constBegin(); it != rotations.constEnd(); ++it) {
        bool ok = false;
        int pin = it.key().toInt(&ok);
        if (!ok) continue;
        manualRotations_[pin] = it.value().toInt();
    }

    QJsonObject colors = root.value("colors").toObject();
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
        bool ok = false;
        int pin = it.key().toInt(&ok);
        if (!ok) continue;
        QColor c(it.value().toString());
        if (c.isValid())
            manualColors_[pin] = c;
    }

    QJsonObject polarities = root.value("polarities").toObject();
    for (auto it = polarities.constBegin(); it != polarities.constEnd(); ++it)  {
        bool ok = false;
        int pin = it.key().toInt(&ok);
        if (!ok) continue;
        manualPolarities_[pin] = it.value().toBool();
    }

    QString boardColorStr = root.value("boardColor").toString();
    if (!boardColorStr.isEmpty()) {
        QColor c(boardColorStr);
        if (c.isValid()) boardColorOverride_ = c;
    }
}

void CanvasWidget::refresh(const std::vector<DetectedComponent>& components) {
    lastComponents_ = components;
    scene_->clear();
    pinItems_.clear();
    componentInfo_.clear();
    draggedItem_ = nullptr; // about to be deleted by scene_->clear()

    drawBoard();

    // Phase 1: create every item and work out its pin-aligned target Y -- heights vary per type, known only post-construction.
    struct Placement {
        const DetectedComponent* comp;
        const ComponentDefinition* def;
        ComponentItem* item;
        float comp_x;
        int comp_w, comp_h;
        float target_y;
        // Set for an I2C daisy-chain link -- Phase 3 positions it relative to this item, comp_x/target_y unused (zeroed).
        ComponentItem* chain_prev;
    };

    // Without this, the two left-side columns land on the same X when every
    // component is the same width (100px), since comp_w cancels out.
    static constexpr float H_GAP = 12.0f;

    std::vector<Placement> placements;
    std::vector<Placement> manualPlacements; // user-dragged -- placed as-is, no auto-stacking
    std::vector<Placement> chainPlacements;  // I2C daisy-chain links after the first -- positioned in Phase 3

    // Tracks the previous device on the shared I2C bus so each chain link finds its predecessor without a
    // separate pass -- relies on detect_oled handing out 900, 901, 902... in the same order as `components`.
    ComponentItem* i2c_chain_prev = nullptr;

    for (const auto& comp : components) {
        if (comp.pin < 0) continue;
        const ComponentDefinition* def = ComponentRegistry::instance().find_by_type(comp.type_name);
        if (!def) continue;

        ComponentItem* item = def->create_item(comp.pin, nullptr);
        item->setDarkTheme(darkTheme_);
        // Must happen before boundingRect() below -- device count changes the item's rendered size.
        item->configureDeviceCount(comp.num_devices);
        item->configureStripLength(comp.strip_length);
        item->configureDisplaySize(comp.display_width, comp.display_height);
        QRectF box = item->boundingRect();
        int comp_w = (int)box.width();
        int comp_h = (int)box.height();

        bool on_i2c_bus = comp.pin >= I2C_BUS_PIN_BASE && comp.pin <= I2C_BUS_PIN_MAX;
        bool is_chain_link = comp.pin > I2C_BUS_PIN_BASE && comp.pin <= I2C_BUS_PIN_MAX;
        ComponentItem* chain_prev = is_chain_link ? i2c_chain_prev : nullptr;

        auto manual = manualPositions_.find(comp.pin);
        if (manual != manualPositions_.end()) {
            manualPlacements.push_back({&comp, def, item, (float)manual->x(), comp_w, comp_h, (float)manual->y(), chain_prev});
            if (on_i2c_bus) i2c_chain_prev = item;
            continue;
        }

        if (is_chain_link) {
            chainPlacements.push_back({&comp, def, item, 0.0f, comp_w, comp_h, 0.0f, chain_prev});
            i2c_chain_prev = item;
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

        placements.push_back({&comp, def, item, comp_x, comp_w, comp_h, target_y, nullptr});
        if (on_i2c_bus) i2c_chain_prev = item; // true here only for the bus's first device
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

        placeComponent(*p.comp, p.def, p.item, p.comp_x, comp_y, p.comp_w, p.comp_h, p.chain_prev);
    }

    for (auto& p : manualPlacements)
        placeComponent(*p.comp, p.def, p.item, p.comp_x, p.target_y, p.comp_w, p.comp_h, p.chain_prev);

    // Phase 3: place I2C daisy-chain links adjacent to their predecessor, now that its final on-screen position is known.
    for (auto& p : chainPlacements) {
        ComponentItem* prev = p.chain_prev;
        float comp_x = prev ? (float)(prev->x() + prev->boundingRect().width() + H_GAP)
                             : (float)(BOARD_X + BOARD_W + 80); // defensive: no predecessor found
        float comp_y = prev ? (float)prev->y() : p.target_y;
        placeComponent(*p.comp, p.def, p.item, comp_x, comp_y, p.comp_w, p.comp_h, prev);
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

void CanvasWidget::updateMatrixRow(int pin, int addr, int row, int bits) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;
    it.value()->updateMatrixRow(addr, row, bits);
}

void CanvasWidget::updateNeopixelShow(int pin, QByteArray rgb) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;
    it.value()->updateStripPixels(rgb);
}

void CanvasWidget::updateOledDisplay(int pin, QByteArray pixels, int width, int height) {
    auto it = pinItems_.find(pin);
    if (it == pinItems_.end()) return;
    it.value()->updateOledFramebuffer(pixels, width, height);
}

void CanvasWidget::onComponentInput(int pin, int eventType, QVariant value) {
    emit inputChanged(pin, eventType, value);
}

void CanvasWidget::drawBoard() {
    QColor base = boardColorOverride_.isValid() ? boardColorOverride_ : profile_.board_color;

    // Dark theme: base is the board bg, chrome shades lighter off it. Light theme: hue forced to a fixed
    // lightness via fromHsl (not .lighter()) so the board bg stays consistently light regardless of the base color.
    QColor board_bg = base;
    if (!darkTheme_) {
        int h, s, l, a;
        base.getHsl(&h, &s, &l, &a);
        board_bg = QColor::fromHsl(h, s, 175, a);
    }

    // Every color below is a shading step off board_bg so chip/pin dots stay distinct and shift together with it.
    auto shade = [&](int amount) {
        return darkTheme_ ? board_bg.lighter(amount) : board_bg.darker(amount);
    };

    const QColor board_border = shade(140);
    const QColor board_label  = shade(350);

    const QColor chip_bg      = shade(115);
    const QColor chip_border  = shade(170);
    const QColor chip_label   = shade(400);

    const QColor pin_dot_bg     = shade(105);
    const QColor pin_dot_border = shade(160);
    const QColor pin_label      = shade(230);

    boardRectItem_ = scene_->addRect(
        BOARD_X, BOARD_Y, BOARD_W, BOARD_H,
        QPen(board_border, 2),
        QBrush(board_bg)
    );

    // Fixed dark color (exposed substrate), same 4-corner placement regardless of size.
    // Inset far enough that they never land on a pin dot, which always sits exactly on the board's x edge.
    {
        int hole_r = 3, hole_inset = 10;
        QPointF holes[4] = {
            QPointF(BOARD_X + hole_inset,           BOARD_Y + hole_inset),
            QPointF(BOARD_X + BOARD_W - hole_inset, BOARD_Y + hole_inset),
            QPointF(BOARD_X + hole_inset,           BOARD_Y + BOARD_H - hole_inset),
            QPointF(BOARD_X + BOARD_W - hole_inset, BOARD_Y + BOARD_H - hole_inset),
        };
        for (const QPointF& h : holes) {
            scene_->addEllipse(
                h.x() - hole_r, h.y() - hole_r, hole_r * 2, hole_r * 2,
                QPen(COLOR_MOUNT_HOLE_BORDER, 1),
                QBrush(COLOR_MOUNT_HOLE)
            );
        }
    }

    // Chip rect position/width computed here (rather than down by the chip
    // rect itself) since the chip label below is placed relative to it.
    int chip_w = BOARD_W - 80;
    int chip_x = BOARD_X + 40;

    // Lit green with a soft glow while running, dim gray otherwise -- same visual language as LedItem's glow.
    int led_d = 10;
    QPointF led_c(BOARD_X + BOARD_W - 35 + led_d / 2.0, BOARD_Y + 5 + led_d / 2.0);
    QColor led_color = sketchRunning_ ? COLOR_POWER_LED_ON : COLOR_POWER_LED_OFF;

    if (sketchRunning_) {
        qreal glow_r = led_d * 1.8;
        QRadialGradient glow(led_c, glow_r);
        QColor g1 = led_color; g1.setAlpha(130);
        QColor g2 = led_color; g2.setAlpha(0);
        glow.setColorAt(0.0, g1);
        glow.setColorAt(1.0, g2);
        scene_->addEllipse(
            led_c.x() - glow_r, led_c.y() - glow_r, glow_r * 2, glow_r * 2,
            QPen(Qt::NoPen), QBrush(glow)
        );
    }

    scene_->addEllipse(
        led_c.x() - led_d / 2.0, led_c.y() - led_d / 2.0, led_d, led_d,
        QPen(led_color.darker(160), 1),
        QBrush(led_color)
    );

    QFont labelFont("Courier New", 9);
    QGraphicsTextItem* label = scene_->addText(profile_.name);
    label->setDefaultTextColor(board_label);
    label->setFont(labelFont);
    // Centered on actual rendered text width, not an assumed one -- board names and widths vary per profile.
    int labelTextW = QFontMetrics(labelFont).horizontalAdvance(profile_.name);
    label->setPos(BOARD_X + BOARD_W / 2.0 - labelTextW / 2.0, BOARD_Y + 20);

    // Chip rect scales with BOARD_W so it stays inside narrower boards
    // (Nano, Teensy) instead of the fixed 120px that only fit the 200px default.
    scene_->addRect(
        chip_x, BOARD_Y + 80, chip_w, 60,
        QPen(chip_border, 1),
        QBrush(chip_bg)
    );
    QFont chipFont("Courier New", 8);
    QGraphicsTextItem* chipLabel = scene_->addText(profile_.chip);
    chipLabel->setDefaultTextColor(chip_label);
    chipLabel->setFont(chipFont);
    // Narrower boards don't fit the full chip name inside the rect at this font size -- drop it below instead of truncating.
    int chipTextW = QFontMetrics(chipFont).horizontalAdvance(profile_.chip);
    if (chipTextW <= chip_w - 16) {
        chipLabel->setPos(chip_x + 8, BOARD_Y + 100);
    } else {
        chipLabel->setPos(chip_x + chip_w / 2.0 - chipTextW / 2.0, BOARD_Y + 80 + 60 + 4);
    }

    // Digital pins -- both below the analog block and (for boards like
    // Teensy 4.1) any extra digital pins above it
    for (int i = 0; i < profile_.pin_count; i++) {
        if (isAnalogPin(i)) continue;
        QPointF pos = pinLocation(i);
        scene_->addEllipse(
            pos.x() - 3, pos.y() - 3, 6, 6,
            QPen(pin_dot_border, 1),
            QBrush(pin_dot_bg)
        );
        QGraphicsTextItem* pinNum = scene_->addText(QString::number(i));
        pinNum->setDefaultTextColor(pin_label);
        pinNum->setFont(QFont("Courier New", 7));
        pinNum->setPos(pos.x() + 6, pos.y() - 8);
    }

    // Analog pins
    for (int i = profile_.analog_offset; i < profile_.analog_offset + profile_.analog_count; i++) {
        QPointF pos = pinLocation(i);
        scene_->addEllipse(
            pos.x() - 3, pos.y() - 3, 6, 6,
            QPen(pin_dot_border, 1),
            QBrush(pin_dot_bg)
        );
        QGraphicsTextItem* pinNum = scene_->addText(QString("A%1").arg(i - 14));
        pinNum->setPlainText(QString("A%1").arg(i - profile_.analog_offset));
        pinNum->setDefaultTextColor(pin_label);
        pinNum->setFont(QFont("Courier New", 7));
        pinNum->setPos(pos.x() - 24, pos.y() - 8);
    }
}

void CanvasWidget::placeComponent(const DetectedComponent& comp, const ComponentDefinition* def,
                                   ComponentItem* item, float comp_x, float comp_y,
                                   int comp_w, int comp_h, ComponentItem* chain_prev) {
    bool is_output = def->is_output;

    item->setPos(comp_x, comp_y);
    scene_->addItem(item);

    // Connect BEFORE anything can emit -- configureMultiPin/emitInitialValue below may emit inputChanged
    // synchronously, and Qt drops signals emitted before a connection exists rather than buffering them.
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

    if (item->supportsRotation()) {
        auto rot = manualRotations_.find(comp.pin);
        if (rot != manualRotations_.end())
            item->configureRotation(rot.value());
    }

    if (item->supportsColorConfig()) {
        auto col = manualColors_.find(comp.pin);
        if (col != manualColors_.end())
            item->configureColor(col.value());
    }

    if (item->supportsPolarity()) {
        auto pol = manualPolarities_.find(comp.pin);
        if (pol != manualPolarities_.end())
            item->configurePolarity(pol.value());
    }

    if (!comp.pin_name.empty()) {
        QGraphicsTextItem* nameText = new QGraphicsTextItem(item);
        nameText->setPlainText(QString::fromStdString(comp.pin_name));
        nameText->setDefaultTextColor(COLOR_COMPONENT_SUBLABEL);
        nameText->setFont(QFont("Courier New", 7));
        nameText->setPos(6, -16);
    }

    std::vector<int> wire_pins;
    if (!comp.pins.empty())
        wire_pins = comp.pins;
    else
        wire_pins = { comp.pin };

    componentInfo_[item] = ComponentInfo{ comp.pin, is_output, wire_pins, def->wire_color, {}, chain_prev };
    updateWires(item);
}

void CanvasWidget::drawWire(QPointF from, QPointF to, const QColor& color,
                             std::vector<QGraphicsLineItem*>& lines) {
    // Fixed semi-transparent black regardless of theme -- keeps bright wire colors visible on a light-mode background.
    QGraphicsLineItem* shadow = scene_->addLine(
        from.x() + 1, from.y() + 1, to.x() + 1, to.y() + 1,
        QPen(QColor(0, 0, 0, 90), 2.5, Qt::SolidLine, Qt::RoundCap)
    );
    lines.push_back(shadow);

    QGraphicsLineItem* line = scene_->addLine(
        from.x(), from.y(), to.x(), to.y(),
        QPen(color, 2, Qt::SolidLine, Qt::RoundCap)
    );
    lines.push_back(line);
}

// Re-derives every wire segment for one component from its current scene position; used for initial placement and dragging.
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

    // I2C daisy-chain link: one short module-to-module jump to the previous device instead of the routed
    // 3-segment board wire below. Lands at local (0,15), the same lead point OledItem::paint() draws at.
    if (info.chain_prev) {
        QPointF comp_edge(comp_x, comp_y + 15.0f);
        QPointF prev_edge(info.chain_prev->x() + info.chain_prev->boundingRect().width(),
                           info.chain_prev->y() + 15.0f);
        drawWire(prev_edge, comp_edge, info.wire_color, info.wire_lines);
        return;
    }

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
            // Turn near the pin, not near the destination -- otherwise the vertical segment cuts through stacked components.
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
        drawWire(target, mid1, info.wire_color, info.wire_lines);
        drawWire(mid1, mid2, info.wire_color, info.wire_lines);
        drawWire(mid2, comp_edge, info.wire_color, info.wire_lines);
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
    // The first device on a shared I2C bus has no real GPIO -- anchor it to the board's SCL pin.
    // Devices after the first (901+) never reach here; they route to the previous device (see updateWires()).
    if (pin == I2C_BUS_PIN_BASE) return pinLocation(profile_.scl_pin);

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