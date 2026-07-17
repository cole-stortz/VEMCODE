#include "src/ui/devicespanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>

static bool parseIntCell(const QString& text, int& out) {
    bool ok = false;
    out = text.trimmed().toInt(&ok, 0); // base 0 auto-detects "0x.." / decimal
    return ok;
}

static std::vector<uint8_t> parseByteList(const QString& text) {
    std::vector<uint8_t> bytes;
    for (const QString& tok : text.split(',', Qt::SkipEmptyParts)) {
        int v;
        if (parseIntCell(tok, v))
            bytes.push_back((uint8_t)(v & 0xFF));
    }
    return bytes;
}

DevicesPanel::DevicesPanel(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(2);
    table_->setHorizontalHeaderLabels({"Address", "Response bytes"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    connect(table_, &QTableWidget::cellChanged, this, &DevicesPanel::onCellChanged);
    layout->addWidget(table_);

    QHBoxLayout* buttonRow = new QHBoxLayout();
    QPushButton* addButton = new QPushButton("+ Add device", this);
    QPushButton* removeButton = new QPushButton("- Remove selected", this);
    addButton->setProperty("role", "outline");
    removeButton->setProperty("role", "outline");
    connect(addButton, &QPushButton::clicked, this, &DevicesPanel::onAddClicked);
    connect(removeButton, &QPushButton::clicked, this, &DevicesPanel::onRemoveClicked);
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);
}

void DevicesPanel::onAddClicked() {
    int row = table_->rowCount();
    table_->blockSignals(true);
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem("0x00"));
    table_->setItem(row, 1, new QTableWidgetItem(""));
    table_->blockSignals(false);
}

void DevicesPanel::onRemoveClicked() {
    int row = table_->currentRow();
    if (row < 0) return;
    int address;
    if (parseIntCell(table_->item(row, 0)->text(), address))
        emit deviceChanged(address, {}); // zero out the address before it drops out of the table
    table_->removeRow(row);
}

void DevicesPanel::onCellChanged(int row, int /*column*/) {
    emitRow(row);
}

void DevicesPanel::emitRow(int row) {
    QTableWidgetItem* addrItem  = table_->item(row, 0);
    QTableWidgetItem* bytesItem = table_->item(row, 1);
    if (!addrItem || !bytesItem) return;

    int address;
    if (!parseIntCell(addrItem->text(), address)) return;

    emit deviceChanged(address, parseByteList(bytesItem->text()));
}

void DevicesPanel::pushAll() {
    for (int row = 0; row < table_->rowCount(); ++row)
        emitRow(row);
}
