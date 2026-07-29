// Standalone paint-function previewer. Builds independently of VEMCODE --
// no ComponentItem/ComponentRegistry includes, just Qt Widgets.
//
// Workflow:
//   1. Copy a component's static paint*() helper (and any static QColor
//      consts it uses) from src/components/*.cpp into the PASTE ZONE below,
//      verbatim.
//   2. Update BOUNDS_W/BOUNDS_H to match that component's boundingRect(),
//      and fix up the call in PreviewWidget::paintEvent to match the
//      helper's signature.
//   3. Build (cmake -B build && cmake --build build) and run ./build/paint_preview.
//   4. Click the shape to toggle "pressed", press 'd' to flip dark/light
//      viewport background (matches CanvasWidget's two themes), scroll to zoom.
//   5. When it looks right, copy the function back into the real .cpp
//      unchanged.
//
// Rendering here mirrors CanvasWidget: QPainter::Antialiasing is on, and
// the viewport background colors below match VIEWPORT_BG_DARK/LIGHT in
// canvaswidget.cpp so gradients/edges read the same as they will on canvas.

#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QRadialGradient>
#include <QLinearGradient>

// ======================== PASTE ZONE START ========================

static const QColor COLOR_SENSOR_FILL("#a874dc");

static void paint(QPainter* p, const QRectF& r, const QColor& lastSensed) {
    p->setPen(QPen(COLOR_SENSOR_FILL.darker(500), 3));
    p->setBrush(QColor(COLOR_SENSOR_FILL.darker(300)));
    p->drawRoundedRect(r, 4, 4);

    QPointF c(r.center().x(), r.top() + r.height() * 0.32);
    qreal lensR = qMin(r.width(), r.height()) * 0.22;
    p->setPen(QPen(COLOR_SENSOR_FILL.darker(500), 1));
    p->setBrush(lastSensed);
    p->drawEllipse(c, lensR, lensR);

    qreal off = qMin(r.width(), r.height()) * 0.3;
    for (const auto& d : {QPointF(-off, -off * 0.6), QPointF(off, -off * 0.6),
                           QPointF(-off, off * 0.6), QPointF(off, off * 0.6)}) {
        p->setPen(QPen(QColor("#3a3a3a").darker(200), 1));
        p->setBrush(QColor("#3a3a3a"));
        p->drawEllipse(c + d, 3, 3);
    }

    // Straight leads on the right edge, one per OUT/S2/S3 pin slot --
    // color sensors are inputs, so CanvasWidget::updateWires attaches
    // wire i at local (width, 15 + i*5), same spacing as WIRE_SPACING.
    p->setPen(QPen(QColor("#999"), 2));
    for (int i = 0; i < 3; ++i) {
        qreal ly = 15 + i * 5;
        p->drawLine(QPointF(r.width() - 5, ly), QPointF(r.width(), ly));
    }
}

// ========================= PASTE ZONE END =========================

// Not part of the paste zone -- stand-ins for the real QLineEdit/
// QGraphicsProxyWidget R/G/B inputs that ColorSensorItem embeds outside of
// paint() (see color_sensor.cpp's constructor). paint() alone never draws
// these, so this fakes their position/size/colors just for visual reference.
// Delete this when swapping the paste zone to a different component.
static void drawColorSensorInputPlaceholders(QPainter* p, const QRectF& r) {
    struct Box { const char* fg; const char* bg; qreal x; };
    for (const auto& b : {Box{"#ff4444", "#1a0000", 4}, Box{"#44ff44", "#001a00", 34}, Box{"#4444ff", "#00001a", 64}}) {
        QRectF box(r.x() + b.x, r.y() + 42, 26, 16);
        p->setPen(QPen(QColor(b.fg), 1));
        p->setBrush(QColor(b.bg));
        p->drawRect(box);
    }
}

static const qreal BOUNDS_W = 100;
static const qreal BOUNDS_H = 64;

static const QColor VIEWPORT_BG_DARK("#1a1a1a");
static const QColor VIEWPORT_BG_LIGHT("#dcdce2");

class PreviewWidget : public QWidget {
    bool pressed_ = false;
    bool dark_ = true;
    qreal zoom_ = 4.0;

public:
    PreviewWidget() {
        setMinimumSize(600, 400);
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), dark_ ? VIEWPORT_BG_DARK : VIEWPORT_BG_LIGHT);

        p.save();
        p.translate(width() / 2.0 - BOUNDS_W * zoom_ / 2.0,
                    height() / 2.0 - BOUNDS_H * zoom_ / 2.0);
        p.scale(zoom_, zoom_);

        QRectF boundsRect(0, 0, BOUNDS_W, BOUNDS_H);
        p.setPen(QPen(Qt::gray, 0.5, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(boundsRect);

        paint(&p, boundsRect, pressed_ ? QColor("#44dc74") : COLOR_SENSOR_FILL);
        drawColorSensorInputPlaceholders(&p, boundsRect);

        p.restore();

        p.setPen(dark_ ? Qt::white : Qt::black);
        p.drawText(10, height() - 10,
                   QString("click: toggle sensed color (%1)   d: theme (%2)   wheel: zoom (%3x)")
                       .arg(pressed_ ? "green" : "default")
                       .arg(dark_ ? "dark" : "light")
                       .arg(zoom_, 0, 'f', 1));
    }

    void mousePressEvent(QMouseEvent*) override {
        pressed_ = true;
        update();
    }

    void mouseReleaseEvent(QMouseEvent*) override {
        pressed_ = false;
        update();
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_D) {
            dark_ = !dark_;
            update();
        }
    }

    void wheelEvent(QWheelEvent* e) override {
        zoom_ *= (e->angleDelta().y() > 0) ? 1.1 : 0.9;
        zoom_ = qBound(0.5, zoom_, 20.0);
        update();
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    PreviewWidget w;
    w.setWindowTitle("Paint Preview");
    w.show();
    return app.exec();
}
