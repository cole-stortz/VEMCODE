#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QMap>
#include <QMouseEvent>
#include <QGraphicsProxyWidget>
#include <QLineEdit>
#include "src/core/circuit/circuitdetector.h"
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
    void buttonPressed(int pin, int value);
    void potentiometerChanged(int pin, int value);
    void pulseInjected(int pin, unsigned long micros);
    void analogInjected(int pin, int value);
    void colorInjected(int out_pin, int s2_pin, int s3_pin, int r, int g, int b);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void drawBoard();
    void drawComponent(const DetectedComponent& comp);
    void drawWire(QPointF from, QPointF to);
    QPointF pinLocation(int pin);
    QColor componentColor(ComponentType type, bool active);

    QGraphicsScene*               scene_;
    QMap<int, QGraphicsRectItem*> pinItems_;
    QMap<int, bool>               buttonStates_;
    QMap<int, bool>               switchStates_;
    QMap<int, ComponentType>      pinTypes_;
    QMap<int, int>                analogValues_;
    QMap<int, QGraphicsTextItem*> servoLabels_;
    QMap<int, QGraphicsTextItem*> lcdRow0Labels_;
    QMap<int, QGraphicsTextItem*> lcdRow1Labels_;

    struct MotorState { int pwm = 0, cwise = 0, anti_cwise = 0; };
    QMap<int, MotorState>         motorStates_;       // keyed by rep pin (comp.pin = PWM pin)
    QMap<int, int>                motorPinToRep_;     // any motor pin → rep pin
    QMap<int, int>                motorCwisePin_;     // rep pin → cwise pin
    QMap<int, int>                motorAntiCwisePin_; // rep pin → anti_cwise pin
    QMap<int, QGraphicsTextItem*> motorLabels_;       // keyed by rep pin

    static constexpr int BOARD_X = 300;
    static constexpr int BOARD_Y = 150;
    static constexpr int BOARD_W = 200;
    int BOARD_H = 300;
    int dragPin_         = -1;
    int dragStartY_      = 0;
    int dragStartValue_  = 512;

    BoardProfile profile_ = BOARD_UNO;

    static constexpr float WIRE_SPACING = 10.0f;
};