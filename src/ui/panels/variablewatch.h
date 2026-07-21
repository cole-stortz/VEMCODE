#pragma once
#include <QWidget>
#include <QTableWidget>
#include <utility>
#include <vector>

// Variable Watch panel: a table of sketch global-variable name -> type,
// polled live from the running sketch DLL by SketchThread. No code changes
// to the sketch are needed -- just type a global's name and pick its type.
class VariableWatch : public QWidget {
    Q_OBJECT

public:
    explicit VariableWatch(QWidget* parent = nullptr);

    // Blanks the Value column on a fresh Run without discarding the
    // user-configured Name/Type rows.
    void clearValues();

signals:
    // Emitted on any Name/Type edit or row add/remove -- name, type ("int"/
    // "float"/"long"/"ulong"/"bool").
    void watchListChanged(std::vector<std::pair<QString, QString>> vars);

public slots:
    void onVariableChanged(QString name, QString value);

private slots:
    void onAddClicked();
    void onRemoveClicked();
    void onCellChanged(int row, int column);

private:
    void emitWatchList();

    QTableWidget* table_;
};
