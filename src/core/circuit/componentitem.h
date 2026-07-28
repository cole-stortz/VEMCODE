#pragma once
#include <QGraphicsObject>
#include <QVariant>
#include <QByteArray>
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

    virtual void configureRotation(int degrees);
    virtual bool supportsRotation() const { return false; }

    virtual bool supportsColorConfig() const { return false; }
    virtual QColor baseColor() const { return QColor(); }
    virtual void configureColor(const QColor& color) {}

    virtual bool supportsPolarity() const { return false; }
    virtual bool isCommonAnode() const { return false; }
    virtual void configurePolarity(bool commonAnode) {}

    // Keypad only: called once, right before configureMultiPin, so the item
    // knows where to split its pins vector into row pins vs. column pins.
    virtual void configureRowsCols(int rows, int cols);

    // Max7219 only: called once, right after construction and before the
    // layout phase queries boundingRect() -- device count changes the
    // item's rendered size (N stacked 8x8 squares), so it must be known
    // before layout math runs, unlike configureRowsCols/configureMultiPin
    // which only affect pin wiring.
    virtual void configureDeviceCount(int count);

    // NeoPixel only: called once, right after construction and before the
    // layout phase queries boundingRect() -- same timing requirement as
    // configureDeviceCount, since pixel count changes the item's rendered
    // size.
    virtual void configureStripLength(int count);

    // NeoPixel only: whole-strip color update, sent once per show(). rgb is
    // count*3 interleaved bytes (r,g,b per pixel), same order the runtime
    // hook hands off.
    virtual void updateStripPixels(const QByteArray& rgb);

    // OLED only: called once, right after construction and before the
    // layout phase queries boundingRect() -- same timing requirement as
    // configureDeviceCount/configureStripLength, since display size changes
    // the item's rendered size.
    virtual void configureDisplaySize(int width, int height);

    // OLED only: whole-framebuffer update, sent once per display(). pixels
    // is width*height bytes, one per pixel (0/1), row-major.
    virtual void updateOledFramebuffer(const QByteArray& pixels, int width, int height);

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