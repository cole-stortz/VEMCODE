#include "src/ui/panels/variablewatch.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>

VariableWatch::VariableWatch(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"Variable", "Type", "Value"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    connect(table_, &QTableWidget::cellChanged, this, &VariableWatch::onCellChanged);
    layout->addWidget(table_);

    QHBoxLayout* buttonRow = new QHBoxLayout();
    QPushButton* addButton = new QPushButton("+ Add variable", this);
    QPushButton* removeButton = new QPushButton("- Remove selected", this);
    addButton->setProperty("role", "outline");
    removeButton->setProperty("role", "outline");
    connect(addButton, &QPushButton::clicked, this, &VariableWatch::onAddClicked);
    connect(removeButton, &QPushButton::clicked, this, &VariableWatch::onRemoveClicked);
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);
}

void VariableWatch::onAddClicked() {
    int row = table_->rowCount();
    table_->blockSignals(true);
    table_->insertRow(row);
    table_->setItem(row, 0, new QTableWidgetItem(""));
    table_->setItem(row, 1, new QTableWidgetItem("int"));
    QTableWidgetItem* valueItem = new QTableWidgetItem("");
    valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
    table_->setItem(row, 2, valueItem);
    table_->blockSignals(false);
}

void VariableWatch::onRemoveClicked() {
    int row = table_->currentRow();
    if (row < 0) return;
    table_->removeRow(row);
    emitWatchList();
}

void VariableWatch::onCellChanged(int row, int column) {
    if (column == 2) return; // Value column is updated programmatically only
    emitWatchList();
}

void VariableWatch::emitWatchList() {
    std::vector<std::pair<QString, QString>> vars;
    for (int row = 0; row < table_->rowCount(); ++row) {
        QTableWidgetItem* nameItem = table_->item(row, 0);
        if (!nameItem || nameItem->text().trimmed().isEmpty()) continue;
        QTableWidgetItem* typeItem = table_->item(row, 1);
        QString type = typeItem ? typeItem->text().trimmed().toLower() : "int";
        vars.push_back({nameItem->text().trimmed(), type});
    }
    emit watchListChanged(vars);
}

void VariableWatch::onVariableChanged(QString name, QString value) {
    for (int row = 0; row < table_->rowCount(); ++row) {
        QTableWidgetItem* nameItem = table_->item(row, 0);
        if (nameItem && nameItem->text().trimmed() == name) {
            table_->item(row, 2)->setText(value);
            return;
        }
    }
}

void VariableWatch::clearValues() {
    for (int row = 0; row < table_->rowCount(); ++row) {
        QTableWidgetItem* item = table_->item(row, 2);
        if (item) item->setText("");
    }
}
