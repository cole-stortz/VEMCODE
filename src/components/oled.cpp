#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include "src/core/circuit/circuitdetector.h"
#include <QPainter>
#include <QImage>
#include <algorithm>
#include <regex>

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
        {},    // detect_pattern -- none, handled by detect_custom below (width/height args, not keyword-matched pins)
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new OledItem(pin, parent);
        }
    };
    def.wire_color = SCREEN_ON;

    // "Adafruit_SSD1306 display(width, height, &Wire, resetPin)" -- wire/reset are optional,
    // so the rest of the arg list is captured as one blob and split by hand.
    def.detect_custom = [](CircuitDetector& ctx, const std::string& source,
                            const std::map<std::string, std::string>& defines,
                            const std::map<std::string, std::vector<int>>&,
                            std::set<int>& claimed) {
        static const std::regex ctor_re(
            R"(\bAdafruit_SSD1306\s+(\w+)\s*(?:=\s*Adafruit_SSD1306\s*)?\(\s*(\w+)\s*,\s*(\w+)\s*(,\s*[^)]*)?\))");

        for (auto it = std::sregex_iterator(source.begin(), source.end(), ctor_re);
             it != std::sregex_iterator(); ++it) {
            std::string obj_name = (*it)[1].str();
            int width  = ctx.resolve_pin((*it)[2].str(), defines);
            int height = ctx.resolve_pin((*it)[3].str(), defines);
            if (width <= 0)  width  = 128;
            if (height <= 0) height = 64;
            width  = std::min(width, 128);
            height = std::min(height, 64);

            int reset_pin = -1;
            if (it->size() > 4 && (*it)[4].matched) {
                std::string rest = (*it)[4].str(); // leading comma + remaining args
                std::vector<std::string> args;
                size_t start = 1; // skip the leading comma already captured
                while (start <= rest.size()) {
                    size_t comma = rest.find(',', start);
                    std::string tok = comma == std::string::npos ? rest.substr(start) : rest.substr(start, comma - start);
                    size_t a = tok.find_first_not_of(" \t");
                    size_t b = tok.find_last_not_of(" \t");
                    args.push_back(a == std::string::npos ? "" : tok.substr(a, b - a + 1));
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
                if (args.size() > 1) reset_pin = ctx.resolve_pin(args[1], defines);
            }

            // No real reset pin: synthetic key from the detector's I2C bus-pin allocator so
            // multiple such OLEDs don't collide (see i2cbus.h).
            int pin_key = reset_pin >= 0 ? reset_pin : ctx.next_i2c_bus_pin();
            if (claimed.count(pin_key) || ctx.pin_already_added(pin_key)) continue;

            DetectedComponent comp;
            comp.type_name      = "OLED";
            comp.pin             = pin_key;
            comp.pins             = {pin_key};
            comp.pin_name         = obj_name;
            comp.confirmed        = false;
            comp.display_width    = width;
            comp.display_height   = height;
            comp.label = "OLED " + obj_name + " (" + std::to_string(width) + "x" + std::to_string(height) +
                         (reset_pin >= 0 ? ", RST=" + std::to_string(reset_pin) : ", no RST") + ")";

            ctx.add_detected_component(comp, claimed);
        }
    };

    ComponentRegistry::instance().register_component(def);
    return true;
}();
