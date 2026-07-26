#pragma once
#include <QWidget>
#include <QVector>
#include <QMap>
#include <QColor>

class QLineEdit;

// A single recorded pin event
struct PinEvent {
    int    pin;
    int    value;      // HIGH=1 LOW=0
    qint64 time_ms;    // milliseconds since simulation started
};

// SignalTimeline renders a logic analyzer style waveform for each pin the
// user has explicitly added to watch (via the pin-number input at the top).
// addEvent() silently ignores pins that haven't been added -- it's called
// unconditionally for every pin change from MainWindow::onPinChanged, so all
// filtering happens here rather than upstream.
// Call clear() when a new sketch starts -- this resets recorded event data
// only, the watched pin list survives across runs.
class SignalTimeline : public QWidget {
    Q_OBJECT

public:
    explicit SignalTimeline(QWidget* parent = nullptr);

    // Record a pin state change -- no-op if pin isn't currently watched
    void addEvent(int pin, int value, qint64 time_ms);

    // Clear recorded event data for all watched pins (keeps the watch list)
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
    void onAddPinClicked();
    void onRemovePinClicked();

    // One track per watched pin -- ordered list of events. A pin present
    // as a key here (even with an empty vector) counts as "watched".
    QMap<int, QVector<PinEvent>> tracks_;

    // Ordered list of watched pins (for consistent track order, and doubles
    // as "is anything being watched at all" for the empty-state message)
    QVector<int> pin_order_;

    QLineEdit* pinInput_ = nullptr;

    qint64 time_window_ms_ = 5000;  // visible window width
    qint64 scroll_offset_ms_ = 0;   // horizontal scroll position

    static constexpr int TRACK_HEIGHT  = 36;
    static constexpr int TRACK_PADDING = 8;
    static constexpr int LABEL_WIDTH   = 60;
    static constexpr int HEADER_HEIGHT = 30; // reserved for the pin input row

    bool dark_ = true;

    QColor trackColor(int pin);
};