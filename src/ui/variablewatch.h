#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QMap>

class VariableWatch : public QWidget {
    Q_OBJECT

public:
    explicit VariableWatch(QWidget* parent = nullptr);
    void clear();

public slots:
    void onVariableChanged(QString name, int value);

private:
    QTableWidget* table_;

    // Maps variable name to its row index in the table
    // so we can update existing rows instead of adding duplicates
    QMap<QString, int> rowMap_;
};