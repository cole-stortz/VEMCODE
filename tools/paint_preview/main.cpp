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

static const QColor DHT_FILL("#74a8dc");

static void paint(QPainter* p, const QRectF& r) {
    QColor base = DHT_FILL;
    p->setPen(QPen(base.darker(180), 3));
    p->setBrush(base);
    p->drawRoundedRect(r, 5, 5);

    // Grille fills the top band only -- the bottom third is left clear for
    // the temp_in_/humid_in_ QLineEdit proxies, which double as the
    // human-readable readout (no point painting a duplicate screen
    // underneath widgets that would just cover it).
    QRectF grille = r.adjusted(6, 6, -6, -r.height() * 0.42);
    p->setPen(QPen(base.darker(160), 1));
    p->setBrush(base.darker(500));
    int cols = 6, rows = 3;
    qreal cw = grille.width() / cols, ch = grille.height() / rows;
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col)
            p->drawRect(QRectF(grille.x() + col * cw + 1, grille.y() + row * ch + 1, cw - 2, ch - 2));

    // Straight lead on the right edge -- DHT is an input, so
    // CanvasWidget::updateWires attaches the wire at local (width, 15).
    p->setPen(QPen(QColor("#999"), 2));
    p->drawLine(QPointF(r.width() - 3, 15), QPointF(r.width()+1, 15));
}

// ========================= PASTE ZONE END =========================

// Not part of the paste zone -- stand-ins for the real QLineEdit/
// QGraphicsProxyWidget temp/humidity inputs that DhtItem embeds outside of
// paint() (see dht.cpp's constructor). paint() alone never draws these, so
// this fakes their position/size/color just for visual reference.
// Delete this when swapping the paste zone to a different component.
static void drawDhtInputPlaceholders(QPainter* p, const QRectF& r) {
    for (qreal x : {4.0, 52.0}) {
        QRectF box(r.x() + x, r.y() + 42, 44, 16);
        p->setPen(QPen(QColor("#74a8dc"), 1));
        p->setBrush(QColor("#0a1420"));
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

        paint(&p, boundsRect);
        drawDhtInputPlaceholders(&p, boundsRect);

        p.restore();

        p.setPen(dark_ ? Qt::white : Qt::black);
        p.drawText(10, height() - 10,
                   QString("d: theme (%1)   wheel: zoom (%2x)")
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
