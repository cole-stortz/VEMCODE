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

// ======================== PASTE ZONE START ========================

static const QColor HBRIDGE_ACTIVE("#dc7474");

static void paint(QPainter* p, const QRectF& r, int pwm, bool cwise, bool antiCwise) {
    // Straight leads on the left edge, one per PWM/ANTI_CWISE/CWISE pin
    // slot -- h-bridge motors are outputs, so CanvasWidget::updateWires
    // attaches wire i at local (0, 15 + i*5), same spacing as WIRE_SPACING.
    p->setPen(QPen(QColor("#999"), 2));
    for (int i = 0; i < 3; ++i) {
        qreal ly = 15 + i * 5;
        p->drawLine(QPointF(10, ly), QPointF(0, ly));
    }
    
    QString dir = (cwise && antiCwise) ? "BRAKE"
                : cwise                 ? "CW"
                : antiCwise             ? "CCW"
                :                         "STOP";

    qreal rad = r.height() * 0.42;
    QPointF c(r.left() + rad + 6, r.center().y());
    QColor body = QColor("#888").lighter(pwm > 0 ? 110 : 80);
    p->setPen(QPen(QColor("#333"), 3));
    p->setBrush(body);
    p->drawEllipse(c, rad, rad);

    p->setPen(QPen(QColor("#222"), 1));
    p->setBrush(QColor("#555"));
    p->drawEllipse(c, rad * 0.18, rad * 0.18);

    if (dir == "CW" || dir == "CCW") {
        p->setPen(QPen(HBRIDGE_ACTIVE, 2));
        QRectF arcRect(c.x() - rad * 0.7, c.y() - rad * 0.7, rad * 1.4, rad * 1.4);
        qreal spanDeg = dir == "CW" ? -270 : 270;
        p->drawArc(arcRect, 0, spanDeg * 16);

        // Arrowhead at the arc's terminal end -- sampled via arcMoveTo so the
        // tangent direction always matches whatever drawArc actually rendered,
        // rather than hand-deriving trig signs for Qt's arc-angle convention.
        QPainterPath tipPath, backPath;
        tipPath.arcMoveTo(arcRect, spanDeg);
        backPath.arcMoveTo(arcRect, spanDeg - (spanDeg > 0 ? 15 : -15));
        QPointF tip = tipPath.currentPosition();
        QLineF dirLine(backPath.currentPosition(), tip);
        dirLine.setLength(1);
        QPointF dirVec = dirLine.p2() - dirLine.p1();
        QPointF normVec(-dirVec.y(), dirVec.x());
        qreal arrowLen = 6;
        QPolygonF arrow;
        arrow << tip + dirVec * arrowLen
              << tip + normVec * (arrowLen * 0.55) - dirVec * (arrowLen * 0.2)
              << tip - normVec * (arrowLen * 0.55) - dirVec * (arrowLen * 0.2);
        p->setPen(Qt::NoPen);
        p->setBrush(HBRIDGE_ACTIVE);
        p->drawPolygon(arrow);
    }

    p->setPen(HBRIDGE_ACTIVE);
    p->setFont(QFont("Courier New", 7));
    qreal textX = r.width() * 0.6;
    qreal textW = r.width() - textX;
    p->drawText(QRectF(textX, r.center().y() - 14, textW, 14), Qt::AlignCenter, dir);
    p->drawText(QRectF(textX, r.center().y(), textW, 14), Qt::AlignCenter, QString("pwm:%1").arg(pwm));
}

// ========================= PASTE ZONE END =========================

static const qreal BOUNDS_W = 100;
static const qreal BOUNDS_H = 54;

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

        paint(&p, boundsRect, pressed_ ? 200 : 0, pressed_, false);

        p.restore();

        p.setPen(dark_ ? Qt::white : Qt::black);
        p.drawText(10, height() - 10,
                   QString("click: toggle spinning (%1)   d: theme (%2)   wheel: zoom (%3x)")
                       .arg(pressed_ ? "CW" : "stopped")
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
