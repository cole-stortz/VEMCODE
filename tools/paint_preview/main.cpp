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

static const QColor LCD_FILL("#cf74dc");

static void paint(QPainter* p, const QRectF& r, const QString& row0, const QString& row1) {
    QColor housing(LCD_FILL.darker(400));
    p->setPen(QPen(housing.darker(180), 3));
    p->setBrush(housing);
    p->drawRoundedRect(r, 4, 4);

    QRectF screen = r.adjusted(r.width() * 0.06, r.height() * 0.18, -r.width() * 0.06, -r.height() * 0.1);
    p->setPen(QPen(QColor("#0a2a1a"), 1));
    p->setBrush(QColor("#4ecb71"));
    p->drawRect(screen);

    p->setPen(QColor("#0a2a1a"));
    p->setFont(QFont("Courier New", 7));
    p->drawText(QRectF(screen.left() + 4, screen.top() + 2, screen.width() - 8, screen.height() / 2 - 2),
                Qt::AlignLeft | Qt::AlignVCenter, row0.left(16));
    p->drawText(QRectF(screen.left() + 4, screen.top() + screen.height() / 2, screen.width() - 8, screen.height() / 2 - 2),
                Qt::AlignLeft | Qt::AlignVCenter, row1.left(16));

    // p->setPen(QPen(QColor("#c0c0c0"), 1));
    // for (int i = 0; i < 6; ++i) {
    //     qreal px = r.left() + 6 + i * (r.width() - 12) / 5.0;
    //     p->drawLine(QPointF(px, r.top()), QPointF(px, r.top() + 4));
    // }

    // Straight leads on the left edge, one per RS/EN/D4-D7 pin slot --
    // LCDs are outputs, so CanvasWidget::updateWires attaches wire i at
    // local (0, 15 + i*5), same spacing as WIRE_SPACING.
    p->setPen(QPen(QColor("#999"), 2));
    for (int i = 0; i < 6; ++i) {
        qreal ly = 15 + i * 5;
        p->drawLine(QPointF(3, ly), QPointF(0, ly));
    }
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

        paint(&p, boundsRect,
              pressed_ ? "HELLO, WORLD!" : QString(16, ' '),
              pressed_ ? "VEMCODE LCD" : QString(16, ' '));

        p.restore();

        p.setPen(dark_ ? Qt::white : Qt::black);
        p.drawText(10, height() - 10,
                   QString("click: toggle sample text (%1)   d: theme (%2)   wheel: zoom (%3x)")
                       .arg(pressed_ ? "on" : "off")
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
