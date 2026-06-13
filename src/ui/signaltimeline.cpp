#include "src/ui/signaltimeline.h"
#include <QPainter>
#include <QPen>
#include <QWheelEvent>
#include <algorithm>

SignalTimeline::SignalTimeline(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background: #1a1a1a;");
    setMinimumHeight(80);
}

void SignalTimeline::addEvent(int pin, int value, qint64 time_ms) {
    if (!tracks_.contains(pin)) {
        tracks_[pin] = QVector<PinEvent>();
        pin_order_.append(pin);
    }
    tracks_[pin].append({pin, value, time_ms}); 

    // Auto-scroll to keep latest events visible
    qint64 latest = time_ms;
    if (latest > scroll_offset_ms_ + time_window_ms_)
        scroll_offset_ms_ = latest - time_window_ms_;

    update();
}

void SignalTimeline::clear() {
    tracks_.clear();
    pin_order_.clear();
    scroll_offset_ms_ = 0;
    update();
}

void SignalTimeline::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    p.fillRect(rect(), QColor("#1a1a1a"));

    if (pin_order_.isEmpty()) {
        p.setPen(QColor("#444"));
        p.drawText(rect(), Qt::AlignCenter, "No signal data yet — run a sketch");
        return;
    }

    p.setPen(QPen(QColor("#2a2a2a"), 1));
    int grid_steps = 10;
    for (int i = 0; i <= grid_steps; i++) {
        int x = LABEL_WIDTH + (w - LABEL_WIDTH) * i / grid_steps;
        p.drawLine(x, 0, x, h);
    }

    int track_index = 0;
    for (int pin : pin_order_) {
        int track_y = track_index * (TRACK_HEIGHT + TRACK_PADDING);
        if (track_y + TRACK_HEIGHT > h) break;

        QColor color = trackColor(pin);

        p.fillRect(0, track_y, w, TRACK_HEIGHT, QColor("#1e1e1e"));

        p.setPen(color);
        p.setFont(QFont("Courier New", 8));
        p.drawText(4, track_y, LABEL_WIDTH - 4, TRACK_HEIGHT,
                   Qt::AlignVCenter, QString("pin %1").arg(pin));

        const auto& events = tracks_[pin];
        if (events.isEmpty()) { track_index++; continue; }

        int signal_w = w - LABEL_WIDTH;
        int y_high   = track_y + TRACK_PADDING;
        int y_low    = track_y + TRACK_HEIGHT - TRACK_PADDING;
        int y_mid    = (y_high + y_low) / 2;

        p.setPen(QPen(color, 1.5));

        // Find the last known state before the visible window starts
        int current_value = 0;
        for (const auto& ev : events) {
            if (ev.time_ms <= scroll_offset_ms_)
                current_value = ev.value;
            else
                break;
        }

        int prev_x = LABEL_WIDTH;
        int prev_y = current_value ? y_high : y_low;

        for (const auto& ev : events) {
            if (ev.time_ms < scroll_offset_ms_) continue;
            if (ev.time_ms > scroll_offset_ms_ + time_window_ms_) break;

            int x     = LABEL_WIDTH + (int)((ev.time_ms - scroll_offset_ms_)
                        * (qint64)signal_w / time_window_ms_);
            int new_y = ev.value ? y_high : y_low;

            // Horizontal segment at current level up to this event
            p.drawLine(prev_x, prev_y, x, prev_y);
            // Vertical transition edge
            p.drawLine(x, y_high, x, y_low);

            prev_x = x;
            prev_y = new_y;
        }

        p.drawLine(prev_x, prev_y, w, prev_y);

        p.setPen(QPen(QColor("#333"), 1));
        p.drawLine(0, track_y + TRACK_HEIGHT, w, track_y + TRACK_HEIGHT);

        track_index++;
    }

    p.setPen(QColor("#555"));
    p.setFont(QFont("Courier New", 7));
    for (int i = 0; i <= grid_steps; i++) {
        int x = LABEL_WIDTH + (w - LABEL_WIDTH) * i / grid_steps;
        qint64 t = scroll_offset_ms_ + time_window_ms_ * i / grid_steps;
        p.drawText(x - 20, h - 12, 40, 12,
                   Qt::AlignCenter, QString("%1ms").arg(t));
    }
}

void SignalTimeline::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y();
        time_window_ms_ = qBound(500LL, time_window_ms_ - delta * 10, 60000LL);
    } else {
        int delta = event->angleDelta().y();
        scroll_offset_ms_ = qMax(0LL, scroll_offset_ms_ - delta * 10);
    }
    update();
}

QColor SignalTimeline::trackColor(int pin) {
    static const QVector<QColor> colors = {
        QColor("#4ec94e"),  // green
        QColor("#4eaaff"),  // blue
        QColor("#ffdd44"),  // yellow
        QColor("#ff8844"),  // orange
        QColor("#ff4e4e"),  // red
        QColor("#cc44ff"),  // purple
        QColor("#44ffcc"),  // teal
        QColor("#ff44aa"),  // pink
    };
    return colors[pin % colors.size()];
}