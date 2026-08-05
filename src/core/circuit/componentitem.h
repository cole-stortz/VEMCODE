#pragma once
#include <QGraphicsObject>
#include <QVariant>
#include <QByteArray>
#include <QColor>
#include <vector>

// Themed like CanvasWidget::drawBoard(): dark theme darkens off the accent,
// light theme forces a consistent light lightness off the same hue instead.
inline QColor themedHousing(const QColor& accent, bool dark, int darkAmount = 300, int lightL = 175) {
    if (dark) return accent.darker(darkAmount);
    int h, s, l, a;
    accent.getHsl(&h, &s, &l, &a);
    return QColor::fromHsl(h, s, lightL, a);
}

// Direction-flipped shading step off a base color -- lighter in dark mode, darker in light mode.
inline QColor themedShade(const QColor& base, int amount, bool dark) {
    return dark ? base.lighter(amount) : base.darker(amount);
}

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

    // Max7219 only: called once after construction, before layout queries boundingRect() --
    // device count changes rendered size (N stacked 8x8), unlike configureRowsCols/configureMultiPin.
    virtual void configureDeviceCount(int count);

    // NeoPixel only: called once after construction, before layout queries boundingRect()
    // -- pixel count changes rendered size (same timing requirement as configureDeviceCount).
    virtual void configureStripLength(int count);

    // NeoPixel only: whole-strip update sent once per show(); rgb is count*3 interleaved
    // bytes (r,g,b per pixel).
    virtual void updateStripPixels(const QByteArray& rgb);

    // OLED only: called once after construction, before layout queries boundingRect() --
    // same timing requirement as configureDeviceCount/configureStripLength.
    virtual void configureDisplaySize(int width, int height);

    // OLED only: whole-framebuffer update sent once per display(); pixels is width*height
    // bytes, one per pixel (0/1), row-major.
    virtual void updateOledFramebuffer(const QByteArray& pixels, int width, int height);

    // Called once by CanvasWidget after inputChanged is connected -- emitting from the
    // constructor or configureMultiPin instead silently drops it (both run before connect).
    virtual void emitInitialValue();

    // Set fresh on each newly-built item -- a theme change triggers a full
    // refresh() rather than updating this on already-placed items.
    void setDarkTheme(bool dark) { darkTheme_ = dark; }

Q_SIGNALS:
    void inputChanged(int pin, int eventType, QVariant value);

protected:
    int pin() const { return pin_; }
    bool isDarkTheme() const { return darkTheme_; }

private:
    int pin_;
    bool darkTheme_ = true;
};