#include "src/ui/variablewatch.h"
#include <QVBoxLayout>
#include <QHeaderView>

VariableWatch::VariableWatch(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(2);
    table_->setHorizontalHeaderLabels({"Variable", "Value"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setStyleSheet(
        "QTableWidget { background: #1e1e1e; color: #d4d4d4;"
        "border: none; font-family: 'Courier New'; font-size: 12px; }"
        "QHeaderView::section { background: #252526; color: #888;"
        "border: none; padding: 4px; }"
    );

    layout->addWidget(table_);
}

void VariableWatch::clear() {
    table_->setRowCount(0);
    rowMap_.clear();
}

void VariableWatch::onVariableChanged(QString name, int value) {
    if (rowMap_.contains(name)) {
        table_->item(rowMap_[name], 1)->setText(QString::number(value));
    } else {
        int row = table_->rowCount();
        table_->insertRow(row);
        table_->setItem(row, 0, new QTableWidgetItem(name));
        table_->setItem(row, 1, new QTableWidgetItem(QString::number(value)));
        rowMap_[name] = row;
    }
}