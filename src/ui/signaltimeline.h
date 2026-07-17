#pragma once
#include <QWidget>
#include <QVector>
#include <QMap>
#include <QColor>

// A single recorded pin event
struct PinEvent {
    int           pin;
    int           value;      // HIGH=1 LOW=0
    qint64        time_ms;    // milliseconds since simulation started
};

// SignalTimeline renders a logic analyzer style waveform for each pin.
// Call addEvent() whenever a pin changes state.
// Call clear() when a new sketch starts.
class SignalTimeline : public QWidget {
    Q_OBJECT

public:
    explicit SignalTimeline(QWidget* parent = nullptr);

    // Record a pin state change
    void addEvent(int pin, int value, qint64 time_ms);

    // Clear all recorded data
    void clear();

    // Set the visible time window in milliseconds (default 5000ms)
    void setTimeWindow(qint64 ms) { time_window_ms_ = ms; update(); }

    // Background/grid/label chrome only -- per-pin trace colors (trackColor)
    // stay fixed in both themes, same as every other component's identity color.
    void setDarkTheme(bool dark) { dark_ = dark; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // One track per pin -- ordered list of events
    QMap<int, QVector<PinEvent>> tracks_;

    // Ordered list of pins seen (for consistent track order)
    QVector<int> pin_order_;

    qint64 time_window_ms_ = 5000;  // visible window width
    qint64 scroll_offset_ms_ = 0;   // horizontal scroll position

    static constexpr int TRACK_HEIGHT  = 36;
    static constexpr int TRACK_PADDING = 8;
    static constexpr int LABEL_WIDTH   = 60;

    bool dark_ = true;

    QColor trackColor(int pin);
};