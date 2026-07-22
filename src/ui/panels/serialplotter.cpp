#include "src/ui/panels/serialplotter.h"
#include <QPainter>
#include <QPen>
#include <QWheelEvent>
#include <QRegularExpression>
#include <QtMath>

SerialPlotter::SerialPlotter(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(80);
}

void SerialPlotter::ingest(const QString& text, qint64 time_ms) {
    line_buf_ += text;
    int nl;
    while ((nl = line_buf_.indexOf('\n')) >= 0) {
        parseLine(line_buf_.left(nl), time_ms);
        line_buf_.remove(0, nl + 1);
    }
    if (!series_order_.isEmpty()) update();
}

void SerialPlotter::addSample(const QString& label, double value, qint64 time_ms) {
    if (!series_.contains(label)) {
        series_[label] = QVector<PlotSample>();
        series_order_.append(label);
    }
    series_[label].append({time_ms, value});

    if (!has_range_) { range_min_ = range_max_ = value; has_range_ = true; }
    else { range_min_ = qMin(range_min_, value); range_max_ = qMax(range_max_, value); }
}

void SerialPlotter::parseLine(const QString& line, qint64 time_ms) {
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) return;

    bool any_added = false;

    if (trimmed.contains(':')) {
        // Handles both the tight Arduino IDE Serial Plotter format
        // ("Wave1:50.0,Wave2:20.0") and the far more common tutorial-sketch
        // style ("x position: 35", "Sensor 1 - Red: 120 | Green: 130") --
        // the label is whatever run of letters/digits/spaces sits directly
        // before a colon, so "Sensor 1 - " (broken by the '-') is dropped
        // but "x position" (no break before the colon) is kept whole.
        static const QRegularExpression label_re(
            R"(([A-Za-z_][\w ]*?)\s*:\s*(-?\d+(?:\.\d+)?))");
        auto it = label_re.globalMatch(trimmed);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            QString label = m.captured(1).trimmed();
            bool ok = false;
            double value = m.captured(2).toDouble(&ok);
            if (!ok || label.isEmpty()) continue;
            addSample(label, value, time_ms);
            any_added = true;
        }
    }

    if (!any_added) {
        // No label found (or no colon at all) -- fall back to Serial
        // Plotter's simplest form: bare numbers separated by whitespace/
        // commas, defaulting to "Value"/"Value 2"/... Tolerates a trailing
        // unit suffix like "35cm" by taking just the leading numeric run.
        static const QRegularExpression sep_re("[\\s,]+");
        static const QRegularExpression lead_num_re(R"(^-?\d+(?:\.\d+)?)");
        QStringList tokens = trimmed.split(sep_re, Qt::SkipEmptyParts);

        QVector<double> values;
        for (const QString& tok : tokens) {
            QRegularExpressionMatch m = lead_num_re.match(tok);
            if (m.hasMatch()) values.append(m.captured(0).toDouble());
        }
        for (int i = 0; i < values.size(); ++i) {
            QString label = values.size() > 1 ? QString("Value %1").arg(i + 1) : "Value";
            addSample(label, values[i], time_ms);
        }
    }

    // Auto-scroll to keep the latest samples visible
    if (time_ms > scroll_offset_ms_ + time_window_ms_)
        scroll_offset_ms_ = time_ms - time_window_ms_;
}

void SerialPlotter::clear() {
    line_buf_.clear();
    series_.clear();
    series_order_.clear();
    scroll_offset_ms_ = 0;
    has_range_ = false;
    update();
}

void SerialPlotter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();

    QColor bg           = dark_ ? QColor("#1a1a1a") : QColor("#eeeef2");
    QColor no_data_text = dark_ ? QColor("#444")    : QColor("#999999");
    QColor grid_color   = dark_ ? QColor("#2a2a2a") : QColor("#d8d8de");
    QColor axis_label   = dark_ ? QColor("#888")    : QColor("#666666");
    QColor time_label   = dark_ ? QColor("#555")    : QColor("#888888");

    p.fillRect(rect(), bg);

    if (series_order_.isEmpty()) {
        p.setPen(no_data_text);
        p.drawText(rect(), Qt::AlignCenter,
                   "No plotted data yet — Serial.println() a number to graph it");
        return;
    }

    // Legend -- series name plus its most recent value, in that series' color
    p.setFont(QFont("Courier New", 8));
    int legend_x = 4;
    for (int i = 0; i < series_order_.size(); ++i) {
        const QString& name = series_order_[i];
        const auto& samples = series_[name];
        double last_value = samples.isEmpty() ? 0.0 : samples.last().value;
        QString text = QString("%1: %2").arg(name).arg(last_value, 0, 'g', 4);
        p.setPen(seriesColor(i));
        QRect bound = p.fontMetrics().boundingRect(text);
        p.drawText(legend_x, 2, bound.width() + 4, LEGEND_HEIGHT - 2,
                   Qt::AlignVCenter, text);
        legend_x += bound.width() + 16;
    }

    int plot_top    = LEGEND_HEIGHT;
    int plot_bottom = h - TIME_LABEL_HEIGHT;
    int plot_left   = LABEL_WIDTH;
    int plot_right  = w;
    int plot_h      = plot_bottom - plot_top;
    int plot_w      = plot_right - plot_left;
    if (plot_h <= 0 || plot_w <= 0) return;

    // Y axis range only grows to fit new samples (see range_min_/range_max_
    // comment in the header) -- no per-frame rescan of visible samples here.
    double min_v = has_range_ ? range_min_ : 0;
    double max_v = has_range_ ? range_max_ : 1;
    if (qFuzzyCompare(min_v + 1.0, max_v + 1.0)) { min_v -= 1; max_v += 1; } // flat-line padding
    double pad = (max_v - min_v) * 0.1;
    min_v -= pad;
    max_v += pad;

    p.setPen(QPen(grid_color, 1));
    static constexpr int TIME_GRID_STEPS  = 10;
    static constexpr int VALUE_GRID_STEPS = 4;
    for (int i = 0; i <= TIME_GRID_STEPS; ++i) {
        int x = plot_left + plot_w * i / TIME_GRID_STEPS;
        p.drawLine(x, plot_top, x, plot_bottom);
    }
    for (int i = 0; i <= VALUE_GRID_STEPS; ++i) {
        int y = plot_top + plot_h * i / VALUE_GRID_STEPS;
        p.drawLine(plot_left, y, plot_right, y);
    }

    p.setFont(QFont("Courier New", 7));
    p.setPen(axis_label);
    for (int i = 0; i <= VALUE_GRID_STEPS; ++i) {
        int y = plot_top + plot_h * i / VALUE_GRID_STEPS;
        double v = max_v - (max_v - min_v) * i / VALUE_GRID_STEPS;
        p.drawText(0, y - 7, LABEL_WIDTH - 4, 14,
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'g', 4));
    }

    auto y_for = [&](double v) {
        return plot_bottom - (int)((v - min_v) * plot_h / (max_v - min_v));
    };
    auto x_for = [&](qint64 t) {
        return plot_left + (int)((t - scroll_offset_ms_) * (qint64)plot_w / time_window_ms_);
    };

    for (int i = 0; i < series_order_.size(); ++i) {
        const auto& samples = series_[series_order_[i]];
        if (samples.isEmpty()) continue;
        p.setPen(QPen(seriesColor(i), 1.5));

        bool have_prev = false;
        int prev_x = 0, prev_y = 0;
        for (const auto& s : samples) {
            if (s.time_ms > scroll_offset_ms_ + time_window_ms_) break;
            int x = x_for(s.time_ms);
            int y = y_for(s.value);
            if (s.time_ms >= scroll_offset_ms_ && have_prev)
                p.drawLine(prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
            have_prev = true;
        }
    }

    p.setFont(QFont("Courier New", 7));
    p.setPen(time_label);
    for (int i = 0; i <= TIME_GRID_STEPS; ++i) {
        int x = plot_left + plot_w * i / TIME_GRID_STEPS;
        qint64 t = scroll_offset_ms_ + time_window_ms_ * i / TIME_GRID_STEPS;
        p.drawText(x - 20, h - TIME_LABEL_HEIGHT + 2, 40, TIME_LABEL_HEIGHT - 2,
                   Qt::AlignCenter, QString("%1ms").arg(t));
    }
}

void SerialPlotter::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y();
        time_window_ms_ = qBound(500LL, time_window_ms_ - delta * 10, 60000LL);
    } else {
        int delta = event->angleDelta().y();
        scroll_offset_ms_ = qMax(0LL, scroll_offset_ms_ - delta * 10);
    }
    update();
}

QColor SerialPlotter::seriesColor(int index) const {
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
    return colors[index % colors.size()];
}
