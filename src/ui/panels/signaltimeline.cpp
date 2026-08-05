#include "src/ui/panels/signaltimeline.h"
#include <QPainter>
#include <QPen>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QIntValidator>

SignalTimeline::SignalTimeline(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(80 + HEADER_HEIGHT);

    QWidget*     headerRow    = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerRow->setFixedHeight(HEADER_HEIGHT);
    headerLayout->setContentsMargins(6, 2, 6, 2);
    headerLayout->setSpacing(4);

    pinInput_ = new QLineEdit(headerRow);
    pinInput_->setPlaceholderText("Pin number...");
    pinInput_->setValidator(new QIntValidator(0, 999, pinInput_));
    pinInput_->setFixedWidth(90);
    headerLayout->addWidget(pinInput_);

    QPushButton* addButton = new QPushButton("+ Add pin", headerRow);
    addButton->setProperty("role", "outline");
    connect(addButton, &QPushButton::clicked, this, &SignalTimeline::onAddPinClicked);
    connect(pinInput_, &QLineEdit::returnPressed, this, &SignalTimeline::onAddPinClicked);
    headerLayout->addWidget(addButton);

    QPushButton* removeButton = new QPushButton("- Remove pin", headerRow);
    removeButton->setProperty("role", "outline");
    connect(removeButton, &QPushButton::clicked, this, &SignalTimeline::onRemovePinClicked);
    headerLayout->addWidget(removeButton);

    headerLayout->addStretch();

    // No child widget below the header on purpose -- paintEvent draws directly onto that region.
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(headerRow);
    layout->addStretch();
}

void SignalTimeline::onAddPinClicked() {
    bool ok = false;
    int pin = pinInput_->text().trimmed().toInt(&ok);
    if (!ok) return;

    if (!tracks_.contains(pin)) {
        tracks_[pin] = QVector<PinEvent>();
        pin_order_.append(pin);
    }
    pinInput_->clear();
    update();
}

void SignalTimeline::onRemovePinClicked() {
    bool ok = false;
    int pin = pinInput_->text().trimmed().toInt(&ok);
    if (!ok) return;

    tracks_.remove(pin);
    pin_order_.removeAll(pin);
    pinInput_->clear();
    update();
}

void SignalTimeline::addEvent(int pin, int value, qint64 time_ms) {
    if (!tracks_.contains(pin)) return; // not watched -- ignore

    tracks_[pin].append({pin, value, time_ms});

    // Auto-scroll to keep latest events visible
    qint64 latest = time_ms;
    if (latest > scroll_offset_ms_ + time_window_ms_)
        scroll_offset_ms_ = latest - time_window_ms_;

    update();
}

void SignalTimeline::clear() {
    for (auto& track : tracks_)
        track.clear();
    scroll_offset_ms_ = 0;
    update();
}

void SignalTimeline::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w   = width();
    int h   = height();
    int top = HEADER_HEIGHT; // waveform area starts below the pin input row

    QColor bg           = dark_ ? QColor("#1a1a1a") : QColor("#eeeef2");
    QColor no_data_text  = dark_ ? QColor("#444")    : QColor("#999999");
    QColor grid_color    = dark_ ? QColor("#2a2a2a") : QColor("#d8d8de");
    QColor track_bg      = dark_ ? QColor("#1e1e1e") : QColor("#f5f5f8");
    QColor track_sep     = dark_ ? QColor("#333")    : QColor("#ccccd2");
    QColor time_label    = dark_ ? QColor("#555")    : QColor("#888888");

    p.fillRect(0, top, w, h - top, bg);

    if (pin_order_.isEmpty()) {
        p.setPen(no_data_text);
        p.drawText(QRect(0, top, w, h - top), Qt::AlignCenter,
                   "No pins tracked — add a pin number above");
        return;
    }

    p.setPen(QPen(grid_color, 1));
    int grid_steps = 10;
    for (int i = 0; i <= grid_steps; i++) {
        int x = LABEL_WIDTH + (w - LABEL_WIDTH) * i / grid_steps;
        p.drawLine(x, top, x, h);
    }

    int track_index = 0;
    for (int pin : pin_order_) {
        int track_y = top + track_index * (TRACK_HEIGHT + TRACK_PADDING);
        if (track_y + TRACK_HEIGHT > h) break;

        QColor color = trackColor(pin);

        p.fillRect(0, track_y, w, TRACK_HEIGHT, track_bg);

        p.setPen(color);
        p.setFont(QFont("Courier New", 8));
        p.drawText(4, track_y, LABEL_WIDTH - 4, TRACK_HEIGHT,
                   Qt::AlignVCenter, QString("pin %1").arg(pin));

        const auto& events = tracks_[pin];
        if (events.isEmpty()) { track_index++; continue; }

        int signal_w = w - LABEL_WIDTH;
        int y_high   = track_y + TRACK_PADDING;
        int y_low    = track_y + TRACK_HEIGHT - TRACK_PADDING;

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

        p.setPen(QPen(track_sep, 1));
        p.drawLine(0, track_y + TRACK_HEIGHT, w, track_y + TRACK_HEIGHT);

        track_index++;
    }

    p.setPen(time_label);
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