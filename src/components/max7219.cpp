#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include "src/core/circuit/circuitdetector.h"
#include <QPainter>
#include <regex>
#include <algorithm>

static const QColor MATRIX_BG    ("#1a1a1a");
static const QColor MATRIX_LIT   ("#dc4a4a");
static const QColor MATRIX_UNLIT ("#3a1414");

// Single device: a square (100px, matching every other component's long side) since an
// 8x8 dot grid reads better square than in the standard 100x44 rect. Daisy-chained devices
// stack as additional 100x100 bands, addr 0 on top (LedControl's addressing order).
class Max7219Item : public ComponentItem {
    static constexpr int MAX_DEVICES = 8;
    int clkPin_ = -1;
    int dinPin_ = -1;
    int numDevices_ = 1;
    bool lit_[MAX_DEVICES][8][8] = {};
    int rotation_ = 0; // 0, 90, 180, 270 for rotation values

    void rotatedSource(int dispRow, int dispCol, int& srcRow, int& srcCol) const {
        constexpr int N = 8;
        switch (rotation_) {
            case 90:
                srcRow = N - 1 - dispCol;
                srcCol = dispRow;
                break;
            case 180:
                srcRow = N - 1 - dispRow;
                srcCol = N - 1 - dispCol;
                break;
            case 270:
                srcRow = dispCol;
                srcCol = N - 1 - dispRow;
                break;
            default: // 0
                srcRow = dispRow;
                srcCol = dispCol;
                break;
        }
    }

public:
    Max7219Item(int pin, QGraphicsItem* parent) : ComponentItem(pin, parent) {}

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 100.0 * numDevices_); }

    void configureDeviceCount(int count) override {
        numDevices_ = count < 1 ? 1 : (count > MAX_DEVICES ? MAX_DEVICES : count);
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        constexpr int   N      = 8;
        constexpr float MARGIN = 8.0f;
        float cell = (100.0f - 2.0f * MARGIN) / N;
        float dotD = cell * 0.7f;

        for (int dev = 0; dev < numDevices_; ++dev) {
            float bandY = dev * 100.0f;

            p->setPen(QPen(QColor("#000000"), 3));
            p->setBrush(MATRIX_BG);
            p->drawRect(QRectF(0, bandY, 100, 100));

            p->setPen(Qt::NoPen);
            for (int row = 0; row < N; ++row) {
                for (int col = 0; col < N; ++col) {
                    int srcRow, srcCol;
                    rotatedSource(row, col, srcRow, srcCol);

                    p->setBrush(lit_[dev][srcRow][srcCol] ? MATRIX_LIT : MATRIX_UNLIT);
                    float cx = MARGIN + col * cell + cell / 2.0f;
                    float cy = bandY + MARGIN + row * cell + cell / 2.0f;
                    p->drawEllipse(QPointF(cx, cy), dotD / 2.0f, dotD / 2.0f);
                }
            }
        }

        // Straight leads on the left edge, one per CS/CLK/DIN slot; wire i attaches at
        // local (0, 15 + i*5), matching WIRE_SPACING.
        p->setPen(QPen(QColor("#999"), 2));
        for (int i = 0; i < 3; ++i) {
            float ly = 15.0f + i * 5.0f;
            p->drawLine(QPointF(3, ly), QPointF(0, ly));
        }
    }

    void configureMultiPin(const std::vector<int>& pins) override {
        if (pins.size() > 1) clkPin_ = pins[1];
        if (pins.size() > 2) dinPin_ = pins[2];
    }

    // Row updates arrive keyed by this item's own pin (CS) once LedControl.h latches a row
    // byte -- CLK/DIN never toggle visible state, so there's no per-bit GUI-thread sampling
    // issue (same limit that ruled out seven-segment multiplexing).
    void updateMatrixRow(int addr, int row, int bits) override {
        if (addr < 0 || addr >= numDevices_ || row < 0 || row > 7) return;
        for (int col = 0; col < 8; ++col)
            lit_[addr][row][col] = (bits >> (7 - col)) & 1;
        update();
    }

    void configureRotation(int degrees) override {
        rotation_ = ((degrees % 360) + 360) % 360;
        update();
    }
    int rotation() const { return rotation_; }
    bool supportsRotation() const override { return true; }
};

static bool registered_max7219 = []() {
    // MAX_CS / MAX_CLK / MAX_DIN style naming -- shared prefix.
    ComponentDefinition prefixed{
        "Max7219",
        {},
        {
            {"CS",  {"CS"}},
            {"CLK", {"CLK"}},
            {"DIN", {"DIN"}},
        },
        {},
        true, // is_output
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new Max7219Item(pin, parent);
        },
        MultiPinStrategy::Prefix,
        "CS"
    };
    prefixed.wire_color = MATRIX_LIT;

    // "LedControl lc(dataPin, clkPin, csPin[, numDevices])" -- positional args, so the
    // generic engine can't find them. Only set here, not on `bare` below, or it'd run twice.
    prefixed.detect_custom = [](CircuitDetector& ctx, const std::string& source,
                                 const std::map<std::string, std::string>& defines,
                                 const std::map<std::string, std::vector<int>>&,
                                 std::set<int>& claimed) {
        static const std::regex ctor_re(
            R"(\bLedControl\s+(\w+)\s*(?:=\s*LedControl\s*)?\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*(?:,\s*(\w+)\s*)?\))");

        for (auto it = std::sregex_iterator(source.begin(), source.end(), ctor_re);
             it != std::sregex_iterator(); ++it) {
            std::string obj_name = (*it)[1].str();
            int dinPin = ctx.resolve_pin((*it)[2].str(), defines);
            int clkPin = ctx.resolve_pin((*it)[3].str(), defines);
            int csPin  = ctx.resolve_pin((*it)[4].str(), defines);
            if (dinPin < 0 || clkPin < 0 || csPin < 0) continue;
            if (claimed.count(csPin) || ctx.pin_already_added(csPin)) continue;

            int num_devices = 1;
            if (it->size() > 5 && (*it)[5].matched) {
                std::string tok = (*it)[5].str();
                try {
                    num_devices = std::stoi(tok);
                } catch (...) {
                    auto dit = defines.find(tok);
                    if (dit != defines.end()) {
                        try { num_devices = std::stoi(dit->second); } catch (...) {}
                    }
                }
            }
            num_devices = std::max(1, std::min(8, num_devices));

            DetectedComponent comp;
            comp.type_name   = "Max7219";
            comp.pin         = csPin;
            comp.pins        = {csPin, clkPin, dinPin};
            comp.pin_name    = obj_name;
            comp.confirmed   = false;
            comp.num_devices = num_devices;
            comp.label = "Max7219 " + obj_name + " (CS=" + std::to_string(csPin) +
                         ", CLK=" + std::to_string(clkPin) + ", DIN=" + std::to_string(dinPin) +
                         (num_devices > 1 ? ", devices=" + std::to_string(num_devices) : "") + ")";

            ctx.add_detected_component(comp, claimed);
        }
    };

    ComponentRegistry::instance().register_component(prefixed);

    // Bare CS/CLK/DIN defines with no shared prefix -- same tradeoff as HBridgeMotor's bare ENA/IN1/IN2 entry.
    ComponentDefinition bare{
        "Max7219",
        {},
        {
            {"CS",  {"CS"}},
            {"CLK", {"CLK"}},
            {"DIN", {"DIN"}},
        },
        {},
        true,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new Max7219Item(pin, parent);
        },
        MultiPinStrategy::Singleton,
        "CS"
    };
    bare.wire_color = MATRIX_LIT;
    ComponentRegistry::instance().register_component(bare);
    return true;
}();
