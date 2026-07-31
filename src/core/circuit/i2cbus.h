#pragma once
// Synthetic pin-key range CircuitDetector hands out to I2C devices with no
// real GPIO to key by (I2C is a shared bus over SDA/SCL, not a per-device
// pin). First device detected in source order gets I2C_BUS_PIN_BASE and
// anchors to the board's real SCL pin; each subsequent one daisy-chains
// off the previous device instead of the board -- see
// CanvasWidget::refresh()'s chain-placement phase and updateWires().
constexpr int I2C_BUS_PIN_BASE = 900;
constexpr int I2C_BUS_PIN_MAX  = 999;
