#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QMap>
#include <QVariant>
#include <QColor>
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
    void setProfile(BoardProfile p) { profile_ = p; BOARD_H = p.pin_count * 14; }

signals:
    void inputChanged(int pin, int eventType, QVariant value);

private slots:
    void onComponentInput(int pin, int eventType, QVariant value);

private:
    void drawBoard();
    void placeComponent(const DetectedComponent& comp, const ComponentDefinition* def,
                         ComponentItem* item, float comp_x, float comp_y, int comp_w, int comp_h);
    void drawWire(QPointF from, QPointF to, const QColor& color);
    QPointF pinLocation(int pin);

    // A pin is "digital" for layout purposes if it's outside the analog
    // range, whether that's below it (the common case) or above it -- some
    // boards (Teensy 4.1: pin_count=42, but analog_offset+analog_count=32)
    // have analog-capable pins that don't cover the whole pin_count, leaving
    // extra digital pins past the analog block.
    bool isAnalogPin(int pin) const;
    int digitalPinIndex(int pin) const; // 0-based position among all digital pins, -1 if not digital
    int digitalPinCount() const;

    QGraphicsScene*             scene_;
    QMap<int, ComponentItem*>   pinItems_;

    static constexpr int BOARD_X = 300;
    static constexpr int BOARD_Y = 150;
    static constexpr int BOARD_W = 200;
    int BOARD_H = 300;

    BoardProfile profile_ = BOARD_UNO;

    static constexpr float WIRE_SPACING = 5.0f;
};