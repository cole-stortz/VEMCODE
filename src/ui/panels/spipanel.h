#pragma once
#include <QWidget>
#include <QLineEdit>
#include <cstdint>
#include <vector>

// Virtual SPI device: SPI has no address field like I2C, so every
// transfer() just cycles through this one configurable byte sequence.
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
