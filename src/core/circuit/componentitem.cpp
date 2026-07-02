#include "componentitem.h"

ComponentItem::ComponentItem(int pin, QGraphicsItem* parent)
    : QGraphicsObject(parent), pin_(pin) {}

void ComponentItem::onPinChanged(int, int) {}
void ComponentItem::updateText(int, const QString&) {}
void ComponentItem::configureMultiPin(const std::vector<int>&) {}