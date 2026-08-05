#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include <QPainter>
#include <QImage>
#include <algorithm>

static const QColor BEZEL_BG   ("#111111");
static const QColor SCREEN_OFF ("#04141a");
static const QColor SCREEN_ON  ("#7fe8ff");

// Rendered as a scaled-up bitmap of the real monochrome framebuffer inside a bezel -- real
// dimensions vary per sketch (128x64, 128x32, ...), so the footprint scales with
// configureDisplaySize() rather than the fixed 100px "long side" (same exception MAX7219's square established).
class OledItem : public ComponentItem {
    static constexpr float PX_SCALE = 1.5f;
    static constexpr float MARGIN = 8.0f;

    int width_ = 128;
    int height_ = 64;
    QImage img_;

    void resetImage() {
        img_ = QImage(width_, height_, QImage::Format_RGB32);
        img_.fill(SCREEN_OFF);
    }

public:
    OledItem(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {
        resetImage();
    }

    void configureDisplaySize(int width, int height) override {
        width_  = width  < 1 ? 1 : width;
        height_ = height < 1 ? 1 : height;
        resetImage();
    }

    QRectF boundingRect() const override {
        return QRectF(0, 0, width_ * PX_SCALE + 2 * MARGIN, height_ * PX_SCALE + 2 * MARGIN);
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF box = boundingRect();
        p->setPen(QPen(QColor("#000000"), 3));
        p->setBrush(BEZEL_BG);
        p->drawRect(box);

        QRectF screenRect(MARGIN, MARGIN, width_ * PX_SCALE, height_ * PX_SCALE);
        p->setRenderHint(QPainter::SmoothPixmapTransform, false);
        p->drawImage(screenRect, img_);

        // Straight lead on the left edge; wire attaches at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(5, 15), QPointF(-1, 15));
    }

    // Whole-framebuffer update sent once per display(); pixels is width*height bytes, one
    // per pixel (0/1), row-major.
    void updateOledFramebuffer(const QByteArray& pixels, int width, int height) override {
        if (width != width_ || height != height_) return; // stale/mismatched flush, ignore
        if (pixels.size() < width * height) return;
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                bool lit = pixels[y * width_ + x] != 0;
                img_.setPixel(x, y, (lit ? SCREEN_ON : SCREEN_OFF).rgb());
            }
        }
        update();
    }
};

static bool registered_oled = []() {
    ComponentDefinition def{
        "OLED",
        {"OLED", "SSD1306", "DISPLAY", "SCREEN"},
        {},    // detect_multi -- none, I2C has no dedicated GPIO pin
        {},    // detect_pattern -- none, handled by CircuitDetector::detect_oled (width/height args, not keyword-matched pins)
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new OledItem(pin, parent);
        }
    };
    def.wire_color = SCREEN_ON;
    ComponentRegistry::instance().register_component(def);
    return true;
}();
