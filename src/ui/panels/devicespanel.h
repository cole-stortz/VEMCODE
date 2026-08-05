#pragma once
#include <QWidget>
#include <QTableWidget>
#include <cstdint>
#include <vector>

// Virtual I2C device panel: a table of 7-bit address -> response byte sequence.
// Wire.requestFrom(addr, n) looks up addr here and returns the configured bytes.
class DevicesPanel : public QWidget {
    Q_OBJECT

public:
    explicit DevicesPanel(QWidget* parent = nullptr);

    // Re-emits deviceChanged for every row; repopulates the runtime's table after a sketch reload wipes RuntimeState.
    void pushAll();

signals:
    void deviceChanged(int address, std::vector<uint8_t> bytes);

private slots:
    void onAddClicked();
    void onRemoveClicked();
    void onCellChanged(int row, int column);

private:
    void emitRow(int row);

    QTableWidget* table_;
};
