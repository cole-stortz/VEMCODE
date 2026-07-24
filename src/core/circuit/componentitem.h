#pragma once
#include <QGraphicsObject>
#include <QVariant>
#include <vector>

enum class ComponentEventType {
    DigitalPress,
    BouncePress,
    AnalogValue,
    PulseUs,
    ColorRGB,
    KeypadWiring,   // {rows, cols, row_pin_0.., col_pin_0..} -- registers the matrix once
    KeypadPress,    // {rowIndex, colIndex, pressed(0/1)}
    DhtReading,     // {temperatureC, humidityPercent}
};

class ComponentItem : public QGraphicsObject {
    Q_OBJECT

public:
    explicit ComponentItem(int pin, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override = 0;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget* = nullptr) override = 0;

    virtual void onPinChanged(int pin, int value);
    virtual void updateText(int row, const QString& text);
    virtual void updateMatrixRow(int addr, int row, int bits);
    virtual void configureMultiPin(const std::vector<int>& pins);

    // Keypad only: called once, right before configureMultiPin, so the item
    // knows where to split its pins vector into row pins vs. column pins.
    virtual void configureRowsCols(int rows, int cols);

    // Max7219 only: called once, right after construction and before the
    // layout phase queries boundingRect() -- device count changes the
    // item's rendered size (N stacked 8x8 squares), so it must be known
    // before layout math runs, unlike configureRowsCols/configureMultiPin
    // which only affect pin wiring.
    virtual void configureDeviceCount(int count);

    // Called once by CanvasWidget after inputChanged is connected. Emitting
    // from the constructor or configureMultiPin instead silently drops it --
    // both run before the caller can connect to a freshly-created item.
    virtual void emitInitialValue();

Q_SIGNALS:
    void inputChanged(int pin, int eventType, QVariant value);

protected:
    int pin() const { return pin_; }

private:
    int pin_;
};