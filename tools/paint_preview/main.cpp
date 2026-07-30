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

// ======================== PASTE ZONE START ========================

static const QColor BEZEL_BG   ("#111111");
static const QColor SCREEN_OFF ("#04141a");
static const QColor SCREEN_ON  ("#7fe8ff");
static constexpr float PX_SCALE = 1.5f;
static constexpr float MARGIN = 8.0f;

static void paint(QPainter* p, int width, int height, const QImage& img) {
    QRectF box(0, 0, width * PX_SCALE + 2 * MARGIN, height * PX_SCALE + 2 * MARGIN);
    p->setPen(QPen(QColor("#000000"), 3));
    p->setBrush(BEZEL_BG);
    p->drawRect(box);

    QRectF screenRect(MARGIN, MARGIN, width * PX_SCALE, height * PX_SCALE);
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);
    p->drawImage(screenRect, img);

    // Straight lead on the left edge -- OLED is an output, so
    // CanvasWidget::updateWires attaches the wire at local (0, 15).
    p->setPen(QPen(QColor("#999"), 2));
    p->drawLine(QPointF(5, 15), QPointF(-1, 15));
}

// ========================= PASTE ZONE END =========================

// Not part of the paste zone -- OledItem's real framebuffer only ever gets
// filled by updateOledFramebuffer() from actual sketch output. This fakes a
// blank vs. lit screen just for visual reference.
// Delete this when swapping the paste zone to a different component.
static QImage makeDemoImage(int width, int height, bool active) {
    QImage img(width, height, QImage::Format_RGB32);
    img.fill(SCREEN_OFF);
    if (active) {
        QPainter ip(&img);
        ip.setPen(SCREEN_ON);
        ip.setFont(QFont("Courier New", 16));
        ip.drawText(img.rect(), Qt::AlignCenter, "OLED");
    }
    return img;
}

static const qreal BOUNDS_W = 128 * PX_SCALE + 2 * MARGIN;
static const qreal BOUNDS_H = 64 * PX_SCALE + 2 * MARGIN;

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

        paint(&p, 128, 64, makeDemoImage(128, 64, pressed_));

        p.restore();

        p.setPen(dark_ ? Qt::white : Qt::black);
        p.drawText(10, height() - 10,
                   QString("click: toggle screen content (%1)   d: theme (%2)   wheel: zoom (%3x)")
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
