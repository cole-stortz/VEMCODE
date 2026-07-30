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

static const QColor KEY_ACTIVE  ("#dc74c2");
static const QColor KEY_INACTIVE("#37102e");
static constexpr int CELL = 26;

// Real 4x4/4x3 membrane keypads (the ones in Arduino starter kits) are
// silkscreened with this exact layout -- match it so the canvas grid reads
// like the physical part, not an arbitrary numbering.
static QString keyLabelFor(int rows, int cols, int r, int c) {
    if (rows == 4 && cols == 4) {
        static const char* K[4][4] = {
            {"1", "2", "3", "A"}, {"4", "5", "6", "B"},
            {"7", "8", "9", "C"}, {"*", "0", "#", "D"},
        };
        return K[r][c];
    }
    if (rows == 4 && cols == 3) {
        static const char* K[4][3] = {
            {"1", "2", "3"}, {"4", "5", "6"},
            {"7", "8", "9"}, {"*", "0", "#"},
        };
        return K[r][c];
    }
    return QString::number(r * cols + c + 1);
}

static void paint(QPainter* p, int rows, int cols, int pressedRow, int pressedCol) {
    if (rows <= 0 || cols <= 0) return;
    QRectF r(0, 0, cols * CELL + 8, rows * CELL + 6);

    p->setPen(QPen(QColor("#000"), 3));
    p->setBrush(QColor("#111"));
    p->drawRoundedRect(r, 4, 4);

    p->setFont(QFont("Courier New", 9));
    for (int rr = 0; rr < rows; ++rr) {
        for (int cc = 0; cc < cols; ++cc) {
            bool active = (rr == pressedRow && cc == pressedCol);
            QRectF cell(4 + cc * CELL, 4 + rr * CELL, CELL - 2, CELL - 2);
            QColor fill = active ? KEY_ACTIVE : KEY_INACTIVE;
            p->setPen(QPen(fill.darker(160), 1));
            p->setBrush(fill);
            p->drawRoundedRect(cell, 3, 3);
            int lum = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
            p->setPen(lum > 128 ? QColor("#1a1a1a") : QColor("#cccccc"));
            p->drawText(cell, Qt::AlignCenter, keyLabelFor(rows, cols, rr, cc));
        }
    }

    // Straight leads on the right edge, one per row/col pin slot -- the
    // same [row_0..row_{rows-1}, col_0..col_{cols-1}] order configureMultiPin
    // uses. Keypads are inputs, so CanvasWidget::updateWires attaches
    // wire i at local (width, 15 + i*5), same spacing as WIRE_SPACING.
    p->setPen(QPen(QColor("#999"), 2));
    for (int i = 0; i < rows + cols; ++i) {
        qreal ly = 15 + i * 5;
        p->drawLine(QPointF(r.width() - 3, ly), QPointF(r.width()+1, ly));
    }
}

// ========================= PASTE ZONE END =========================

static const qreal BOUNDS_W = 4 * CELL + 8;
static const qreal BOUNDS_H = 4 * CELL + 8;

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

        paint(&p, 4, 4, pressed_ ? 1 : -1, pressed_ ? 2 : -1);

        p.restore();

        p.setPen(dark_ ? Qt::white : Qt::black);
        p.drawText(10, height() - 10,
                   QString("click: toggle a key pressed (%1)   d: theme (%2)   wheel: zoom (%3x)")
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
