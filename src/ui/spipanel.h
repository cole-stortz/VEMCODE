#pragma once
#include <QWidget>
#include <QLineEdit>
#include <cstdint>
#include <vector>

// Virtual SPI device: a single configurable response byte sequence. SPI has
// no address field like I2C's requestFrom (device selection is a plain
// digitalWrite on a CS pin, up to the sketch) -- so every SPI.transfer() call
// just consumes the next byte from this sequence, cycling back to the start
// when exhausted.
class SpiPanel : public QWidget {
    Q_OBJECT

public:
    explicit SpiPanel(QWidget* parent = nullptr);

    // Re-emits bytesChanged with the current field -- used to repopulate the
    // runtime's response sequence after a sketch reload wipes RuntimeState.
    void pushAll();

signals:
    void bytesChanged(std::vector<uint8_t> bytes);

private slots:
    void onTextChanged(const QString& text);

private:
    QLineEdit* bytesEdit_;
};
