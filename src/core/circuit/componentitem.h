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
};

class ComponentItem : public QGraphicsObject {
    Q_OBJECT

public:
    explicit ComponentItem(int pin, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override = 0;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget* = nullptr) override = 0;

    virtual void onPinChanged(int pin, int value);
    virtual void updateText(int row, const QString& text);
    virtual void configureMultiPin(const std::vector<int>& pins);

Q_SIGNALS:
    void inputChanged(int pin, int eventType, QVariant value);

protected:
    int pin() const { return pin_; }

private:
    int pin_;
};