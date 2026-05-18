#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QMap>
#include "src/core/circuit/circuitdetector.h"

// CanvasWidget renders the circuit diagram detected from the sketch.
// It owns a QGraphicsScene and populates it with:
//   - An Arduino Uno board graphic in the center
//   - A component item for each DetectedComponent
//   - A wire connecting each component to its pin on the board
//
// Call refresh() after CircuitDetector::detect() to rebuild the scene.
// Call updatePin() when a pin changes state during simulation.
class CanvasWidget : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);

    // Rebuild the scene from a fresh detection result
    void refresh(const std::vector<DetectedComponent>& components);

    // Update a pin's visual state (HIGH/LOW) during simulation
    void updatePin(int pin, int value);

private:
    void drawBoard();
    void drawComponent(const DetectedComponent& comp, int index, int total);
    void drawWire(QPointF from, QPointF to);

    // Returns the board-edge point for a given pin number
    QPointF pinLocation(int pin);

    // Returns color for a component type
    QColor componentColor(ComponentType type, bool active);

    QGraphicsScene* scene_;

    // Maps pin number → component rect item so we can update color at runtime
    QMap<int, QGraphicsRectItem*> pinItems_;

    // Board dimensions and position
    static constexpr int BOARD_X      = 300;
    static constexpr int BOARD_Y      = 150;
    static constexpr int BOARD_W      = 200;
    static constexpr int BOARD_H      = 300;
};