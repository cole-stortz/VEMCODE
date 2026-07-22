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

// SerialPlotter graphs numeric values printed via Serial.println(), matching
// the Arduino IDE Serial Plotter protocol: each line is one or more tokens
// separated by whitespace/commas, each either "label:value" or a bare number
// (bare numbers get default names "Value"/"Value 2"/...). Each label becomes
// its own scrolling line trace sharing one auto-scaled Y axis.
class SerialPlotter : public QWidget {
    Q_OBJECT

public:
    explicit SerialPlotter(QWidget* parent = nullptr);

    // Feed raw Serial output text as it arrives -- buffers partial lines
    // internally (Serial.print() calls without a trailing '\n' are common)
    // and only parses once a '\n' completes a line.
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

    // Y axis range only ever grows to fit new samples, never shrinks back
    // down when values settle -- constant rescaling reads as an annoying
    // pulse, and it's rare that a real signal's range needs to be reclaimed.
    double range_min_ = 0;
    double range_max_ = 0;
    bool   has_range_ = false;

    static constexpr int LEGEND_HEIGHT     = 20;
    static constexpr int LABEL_WIDTH       = 50;
    static constexpr int TIME_LABEL_HEIGHT = 16;

    bool dark_ = true;
};
