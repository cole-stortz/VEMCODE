#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QMap>

// Variable Watch Panel: 
// - Watches variable changes and sets new values
// - Sets panel information to be drawn inside Debug section.
class VariableWatch : public QWidget {
    Q_OBJECT

public:
    // Set panel info
    explicit VariableWatch(QWidget* parent = nullptr);  
    // Clear helper method
    void clear();

public slots:
    // Watch for inputed variable changes and set the new values
    void onVariableChanged(QString name, int value);   

private:
    QTableWidget* table_; // Initialize panel

    // Maps variable name to its row index in the table
    // so we can update existing rows instead of adding duplicates
    QMap<QString, int> rowMap_;
};