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
#include <QPainterPath>
#include <QLineF>
#include <QImage>
#include <QFont>
#include <QtMath>
#include <cmath>

// ======================== PASTE ZONE START ========================

static const QColor SERVO_FILL("#74dc8e");

static void paint(QPainter* p, const QRectF& r, int angle) {
    QRectF body = r.adjusted(2, 2, -2, -2);
    p->setPen(QPen(SERVO_FILL.darker(180), 3));
    p->setBrush(SERVO_FILL.darker(400));
    p->drawRoundedRect(body, 4, 4);

    QPointF pivot(body.left() + 20, r.center().y());
    qreal hornLen = 16;

    p->setPen(QPen(QColor("#111"), 1));
    p->setBrush(QColor("#333"));
    p->drawEllipse(pivot, 6, 6);

    qreal a = qDegreesToRadians(angle - 90.0);
    QPointF tip = pivot + QPointF(std::cos(a) * hornLen, std::sin(a) * hornLen);
    p->setPen(QPen(SERVO_FILL, 3));
    p->drawLine(pivot, tip);
    p->setPen(Qt::NoPen);
    p->setBrush(SERVO_FILL);
    p->drawEllipse(tip, 4, 4);

    p->setPen(SERVO_FILL);
    p->setFont(QFont("Courier New", 8));
    qreal textX = pivot.x() + hornLen + 10;
    p->drawText(QRectF(textX, 0, r.width() - textX, r.height()), Qt::AlignCenter, QString("%1°").arg(angle));

    p->setPen(QPen(QColor("#999"), 2));
    p->drawLine(QPointF(5, 15), QPointF(0, 15));

}

// ========================= PASTE ZONE END =========================

static const qreal BOUNDS_W = 100;
static const qreal BOUNDS_H = 44;

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

        paint(&p, boundsRect, pressed_ ? 180 : 0);

        p.restore();

        p.setPen(dark_ ? Qt::white : Qt::black);
        p.drawText(10, height() - 10,
                   QString("click: toggle angle (%1)   d: theme (%2)   wheel: zoom (%3x)")
                       .arg(pressed_ ? "180" : "0")
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
