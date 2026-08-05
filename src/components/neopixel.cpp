#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include "src/core/circuit/circuitdetector.h"
#include <QPainter>
#include <algorithm>
#include <regex>

static const QColor STRIP_BG ("#1a1a1a");
static const QColor STRIP_OFF("#2a2a2a");
static const QColor NEOPIXEL_ACCENT("#4ade80");

// Rendered as a grid of dots wrapping every COLS pixels within the standard 100px-wide
// footprint, growing downward as pixel count increases (same shape MAX7219 uses for its device chain).
class NeoPixelItem : public ComponentItem {
    static constexpr int MAX_PIXELS = 256;
    static constexpr int COLS = 10;
    int pixelCount_ = 1;
    QColor pixels_[MAX_PIXELS];

public:
    NeoPixelItem(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {
        std::fill(std::begin(pixels_), std::end(pixels_), STRIP_OFF);
    }

    void configureStripLength(int count) override {
        pixelCount_ = count < 1 ? 1 : (count > MAX_PIXELS ? MAX_PIXELS : count);
    }

    QRectF boundingRect() const override {
        int rows = (pixelCount_ + COLS - 1) / COLS;
        return QRectF(0, 0, 100, 16.0 * rows + 8.0);
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF box = boundingRect();
        p->setPen(QPen(QColor("#000000"), 3));
        p->setBrush(STRIP_BG);
        p->drawRect(box);

        constexpr float MARGIN = 8.0f;
        float cell = (100.0f - 2.0f * MARGIN) / COLS;
        float dotD = cell * 0.7f;

        p->setPen(Qt::NoPen);
        for (int i = 0; i < pixelCount_; ++i) {
            int row = i / COLS;
            int col = i % COLS;
            float cx = MARGIN + col * cell + cell / 2.0f;
            float cy = MARGIN + row * cell + cell / 2.0f;
            p->setBrush(pixels_[i]);
            p->drawEllipse(QPointF(cx, cy), dotD / 2.0f, dotD / 2.0f);
        }

        // Straight lead on the left edge; wire attaches at local (0, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(5, 15), QPointF(0, 15));
    }

    // Whole-strip update sent once per show(); rgb is pixelCount_*3 interleaved bytes.
    void updateStripPixels(const QByteArray& rgb) override {
        int n = std::min(pixelCount_, (int)(rgb.size() / 3));
        for (int i = 0; i < n; ++i) {
            auto r = (uchar)rgb[i * 3 + 0];
            auto g = (uchar)rgb[i * 3 + 1];
            auto b = (uchar)rgb[i * 3 + 2];
            pixels_[i] = QColor(r, g, b);
        }
        update();
    }
};

static bool registered_neopixel = []() {
    ComponentDefinition def{
        "NeoPixel",
        {"NEOPIXEL", "WS2812", "PIXELS", "PIXEL", "STRIP"},
        {},    // detect_multi -- none, single-pin component
        {},    // detect_pattern -- none, handled by detect_custom below (1st arg is LED count, not a keyword-matched pin)
        true,  // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new NeoPixelItem(pin, parent);
        }
    };
    def.wire_color = NEOPIXEL_ACCENT;

    // "Adafruit_NeoPixel strip(count, pin[, type])" -- optional 3rd arg (color order/speed
    // flags) is matched loosely since it's often an expression like "NEO_GRB + NEO_KHZ800".
    def.detect_custom = [](CircuitDetector& ctx, const std::string& source,
                            const std::map<std::string, std::string>& defines,
                            const std::map<std::string, std::vector<int>>&,
                            std::set<int>& claimed) {
        static const std::regex ctor_re(
            R"(\bAdafruit_NeoPixel\s+(\w+)\s*(?:=\s*Adafruit_NeoPixel\s*)?\(\s*(\w+)\s*,\s*(\w+)\s*(?:,[^)]*)?\))");

        for (auto it = std::sregex_iterator(source.begin(), source.end(), ctor_re);
             it != std::sregex_iterator(); ++it) {
            std::string obj_name = (*it)[1].str();
            int count = ctx.resolve_pin((*it)[2].str(), defines);
            int pin   = ctx.resolve_pin((*it)[3].str(), defines);
            if (pin < 0) continue;
            if (claimed.count(pin) || ctx.pin_already_added(pin)) continue;

            int strip_length = count < 1 ? 1 : std::min(count, 256);

            DetectedComponent comp;
            comp.type_name    = "NeoPixel";
            comp.pin          = pin;
            comp.pins         = {pin};
            comp.pin_name     = obj_name;
            comp.confirmed    = false;
            comp.strip_length = strip_length;
            comp.label = "NeoPixel " + obj_name + " (pin " + std::to_string(pin) +
                         ", pixels=" + std::to_string(strip_length) + ")";

            ctx.add_detected_component(comp, claimed);
        }
    };

    ComponentRegistry::instance().register_component(def);
    return true;
}();
