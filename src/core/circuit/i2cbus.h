#pragma once
// Synthetic pin-key range for I2C devices with no real GPIO to key by (I2C is a shared
// SDA/SCL bus, not per-device pins). First device anchors to the board's real SCL pin;
// each subsequent one daisy-chains off the previous device (see CanvasWidget::refresh()).
constexpr int I2C_BUS_PIN_BASE = 900;
constexpr int I2C_BUS_PIN_MAX  = 999;
