#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QMap>
#include <QVariant>
#include <QColor>
#include <QByteArray>
#include <vector>
#include "src/core/circuit/circuitdetector.h"
#include "src/core/circuit/componentitem.h"
#include "src/core/runtime/boardprofile.h"

class CanvasWidget : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);
    void refresh(const std::vector<DetectedComponent>& components);
    void updatePin(int pin, int value);
    void updateLcdText(int pin, int row, const QString& text);
    void updateMatrixRow(int pin, int addr, int row, int bits);
    void updateNeopixelShow(int pin, QByteArray rgb);
    void updateOledDisplay(int pin, QByteArray pixels, int width, int height);
    void setProfile(BoardProfile p);

    // Layout mode: components become draggable instead of receiving their
    // normal interactive mouse events (button press, slider drag, etc).
    void setLayoutMode(bool on);
    bool layoutMode() const { return layoutMode_; }

    // Drops all dragged positions and re-runs auto-placement.
    void resetLayout();

    // Persists dragged positions + zoom to "<sketchPath>.vblayout". Called on
    // explicit Save/Save As only -- not autosaved.
    void saveLayout(const QString& sketchPath) const;

    // Loads "<sketchPath>.vblayout" if present, else clears to defaults; call when switching sketches.
    // Positions apply on the next refresh(); zoom applies now.
    void loadLayout(const QString& sketchPath);

    void zoomIn();
    void zoomOut();

    // Only affects the viewport background outside the board -- board/chip/pin chrome and component colors are fixed.
    void setDarkTheme(bool dark);
    bool isDarkTheme() const { return darkTheme_; }

    // Drives the board's power-indicator LED -- lit while running, dim otherwise.
    void setSketchRunning(bool running);

signals:
    void inputChanged(int pin, int eventType, QVariant value);

private slots:
    void onComponentInput(int pin, int eventType, QVariant value);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void drawBoard();
    // `chain_prev` set for an I2C daisy-chain link (see i2cbus.h) -- its wire attaches to that item, not a board pin.
    void placeComponent(const DetectedComponent& comp, const ComponentDefinition* def,
                         ComponentItem* item, float comp_x, float comp_y, int comp_w, int comp_h,
                         ComponentItem* chain_prev = nullptr);
    // Draws a wire plus a shadow behind it (keeps bright colors visible in light mode); both appended to `lines`.
    void drawWire(QPointF from, QPointF to, const QColor& color,
                  std::vector<QGraphicsLineItem*>& lines);
    void updateWires(ComponentItem* item); // re-route a component's wires from its current position
    QPointF pinLocation(int pin);
    void setZoom(qreal zoom);
    void applyThemeStyle(); // pushes viewport background for the current theme

    // "Digital" = outside the analog range, above or below it -- Teensy 4.1's
    // analog block doesn't cover the whole pin_count, leaving digital pins past it.
    bool isAnalogPin(int pin) const;
    int digitalPinIndex(int pin) const; // 0-based position among all digital pins, -1 if not digital
    int digitalPinCount() const;

    QGraphicsScene*             scene_;
    QMap<int, ComponentItem*>   pinItems_;

    // Per-component wire routing info so a drag can re-route without touching the rest of the circuit.
    struct ComponentInfo {
        int primary_pin;
        bool is_output;
        std::vector<int> wire_pins;
        QColor wire_color;
        std::vector<QGraphicsLineItem*> wire_lines;
        // Set for an I2C daisy-chain link -- updateWires() routes its wire
        // to this item's edge instead of pinLocation(wire_pins[0]).
        ComponentItem* chain_prev = nullptr;
    };
    QMap<ComponentItem*, ComponentInfo> componentInfo_;

    bool layoutMode_ = false;
    ComponentItem* draggedItem_ = nullptr;
    QPointF dragOffset_;
    // Positions the user has dragged to, keyed by primary pin. Survive refresh()
    // within the running session so re-running the sketch doesn't undo a drag.
    QMap<int, QPointF> manualPositions_;
    std::vector<DetectedComponent> lastComponents_; // cached for resetLayout()
    QHash<int, int> manualRotations_; // pin -> rotation degrees 

    QHash<int, QColor> manualColors_; // pin -> base LED color

    QHash<int, bool> manualPolarities_; // pin -> isCommonAnode

    // Per-sketch override of the board's fill color, set via ctrl+click on
    // the board itself. Invalid (default QColor()) means "use profile_.board_color".
    QColor boardColorOverride_;
    QGraphicsRectItem* boardRectItem_ = nullptr; // for ctrl+click hit-testing in mousePressEvent

    qreal zoomLevel_ = 1.0;
    static constexpr qreal ZOOM_MIN = 0.25;
    static constexpr qreal ZOOM_MAX = 3.0;

    bool darkTheme_ = true;
    bool sketchRunning_ = false;

    static constexpr int BOARD_X = 300;
    static constexpr int BOARD_Y = 150;
    int BOARD_W = 200;
    int BOARD_H = 300;

    BoardProfile profile_ = BOARD_UNO;

    static constexpr float WIRE_SPACING = 5.0f;
};