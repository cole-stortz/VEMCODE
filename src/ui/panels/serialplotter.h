#pragma once
#include <QWidget>
#include <QVector>
#include <QMap>
#include <QColor>
#include <QString>

// A single recorded numeric sample for one plotted series.
struct PlotSample {
    qint64 time_ms;
    double value;
};

// Graphs numeric values from Serial.println(), matching the Arduino IDE Serial Plotter
// protocol: whitespace/comma-separated "label:value" or bare number tokens per line.
class SerialPlotter : public QWidget {
    Q_OBJECT

public:
    explicit SerialPlotter(QWidget* parent = nullptr);

    // Feed raw Serial output as it arrives; buffers partial lines and only parses on '\n'.
    void ingest(const QString& text, qint64 time_ms);

    // Clear all recorded data -- call when a new sketch starts.
    void clear();

    // Set the visible time window in milliseconds (default 5000ms)
    void setTimeWindow(qint64 ms) { time_window_ms_ = ms; update(); }

    void setDarkTheme(bool dark) { dark_ = dark; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void parseLine(const QString& line, qint64 time_ms);
    void addSample(const QString& label, double value, qint64 time_ms);
    QColor seriesColor(int index) const;

    QString line_buf_; // accumulates text until a '\n' completes a line

    // One track per named series -- ordered list of samples
    QMap<QString, QVector<PlotSample>> series_;

    // Ordered list of series names seen (for consistent legend/color order)
    QVector<QString> series_order_;

    qint64 time_window_ms_   = 5000; // visible window width
    qint64 scroll_offset_ms_ = 0;    // horizontal scroll position

    // Y axis range only grows, never shrinks -- constant rescaling reads as an annoying pulse.
    double range_min_ = 0;
    double range_max_ = 0;
    bool   has_range_ = false;

    static constexpr int LEGEND_HEIGHT     = 20;
    static constexpr int LABEL_WIDTH       = 50;
    static constexpr int TIME_LABEL_HEIGHT = 16;

    bool dark_ = true;
};
