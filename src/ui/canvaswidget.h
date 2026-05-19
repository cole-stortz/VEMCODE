#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QMap>
#include <QMouseEvent>
#include "src/core/circuit/circuitdetector.h"

class CanvasWidget : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);
    void refresh(const std::vector<DetectedComponent>& components);
    void updatePin(int pin, int value);

signals:
    void buttonPressed(int pin, int value);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void drawBoard();
    void drawComponent(const DetectedComponent& comp, int index, int total);
    void drawWire(QPointF from, QPointF to);
    QPointF pinLocation(int pin);
    QColor componentColor(ComponentType type, bool active);

    QGraphicsScene*              scene_;
    QMap<int, QGraphicsRectItem*> pinItems_;
    QMap<int, bool>               buttonStates_;
    QMap<int, ComponentType>      pinTypes_;

    static constexpr int BOARD_X = 300;
    static constexpr int BOARD_Y = 150;
    static constexpr int BOARD_W = 200;
    static constexpr int BOARD_H = 300;
};