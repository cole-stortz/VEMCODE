#include "src/core/circuit/componentitem.h"
#include "src/core/circuit/componentregistry.h"
#include "src/core/circuit/circuitdetector.h"
#include <QPainter>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include <regex>

static const QColor DHT_FILL("#5c8ee8");

class DhtItem : public ComponentItem {
    QLineEdit* temp_in_;
    QLineEdit* humid_in_;

public:
    DhtItem(int pin, QGraphicsItem* parent)
        : ComponentItem(pin, parent) {
        auto make_input = [this](const char* text, const char* fg, const char* bg, int x) -> QLineEdit* {
            QLineEdit* in = new QLineEdit(text);
            in->setFixedSize(44, 16);
            in->setStyleSheet(QString("background:%1; color:%2; border:1px solid %2;")
                              .arg(bg).arg(fg));
            auto* proxy = new QGraphicsProxyWidget(this);
            proxy->setWidget(in);
            proxy->setPos(x, 42);
            return in;
        };

        temp_in_  = make_input("22.0", "#ff8a65", "#1a0d08", 4);
        humid_in_ = make_input("50.0", "#74a8dc", "#0a1420", 52);

        connect(temp_in_,  &QLineEdit::textChanged, this, [this] { emitReading(); });
        connect(humid_in_, &QLineEdit::textChanged, this, [this] { emitReading(); });
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 100, 64); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRectF r = boundingRect();
        QColor base = DHT_FILL;
        p->setPen(QPen(base.darker(180), 3));
        p->setBrush(base);
        p->drawRoundedRect(r, 5, 5);

        // Grille fills the top band only; the bottom third is left clear for the
        // temp_in_/humid_in_ QLineEdit proxies, which double as the readout.
        QRectF grille = r.adjusted(6, 6, -6, -r.height() * 0.42);
        p->setPen(QPen(base.darker(160), 1));
        p->setBrush(base.darker(500));
        int cols = 6, rows = 3;
        qreal cw = grille.width() / cols, ch = grille.height() / rows;
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col)
                p->drawRect(QRectF(grille.x() + col * cw + 1, grille.y() + row * ch + 1, cw - 2, ch - 2));

        // Straight lead on the right edge; wire attaches at local (width, 15).
        p->setPen(QPen(QColor("#999"), 2));
        p->drawLine(QPointF(r.width() - 3, 15), QPointF(r.width() + 1, 15));
    }

    void emitInitialValue() override { emitReading(); }

private:
    void emitReading() {
        bool tok, hok;
        double t = temp_in_->text().toDouble(&tok);
        double h = qBound(0.0, humid_in_->text().toDouble(&hok), 100.0);
        if (tok && hok)
            emit inputChanged(pin(), (int)ComponentEventType::DhtReading, QVariantList{t, h});
    }
};

static bool reg_dht = []() {
    ComponentDefinition def{
        "DHT",
        {"DHT", "DHTPIN", "DHT_PIN"},
        {}, {}, false,
        [](int pin, QGraphicsItem* parent) -> ComponentItem* {
            return new DhtItem(pin, parent);
        }
    };
    def.wire_color = DHT_FILL;

    // "DHT dht(DHTPIN, DHTTYPE)" -- 2nd arg is a sensor-type selector, not a pin, so the
    // generic "every arg is a pin" constructor-pattern engine can't be used.
    def.detect_custom = [](CircuitDetector& ctx, const std::string& source,
                            const std::map<std::string, std::string>& defines,
                            const std::map<std::string, std::vector<int>>&,
                            std::set<int>& claimed) {
        static const std::regex ctor_re(
            R"(\bDHT\s+(\w+)\s*\(\s*(\w+)\s*(?:,\s*(\w+)\s*)?\))");
        auto it = std::sregex_iterator(source.begin(), source.end(), ctor_re);
        auto end_it = std::sregex_iterator();
        if (it == end_it) return;

        std::string obj_name  = (*it)[1].str();
        std::string pin_token = (*it)[2].str();
        std::string type_token = it->size() > 3 ? (*it)[3].str() : "";

        int pin = ctx.resolve_pin(pin_token, defines);
        if (pin < 0 || claimed.count(pin) || ctx.pin_already_added(pin)) return;

        std::string type_label;
        auto type_it = defines.find(type_token);
        if (type_it != defines.end()) type_label = type_it->second;

        DetectedComponent comp;
        comp.type_name = "DHT";
        comp.pin       = pin;
        comp.pins      = {pin};
        comp.pin_name  = obj_name;
        comp.confirmed = false;
        comp.label = "DHT" + (type_label.empty() ? "" : " " + type_label) +
                     " " + obj_name + " (pin " + std::to_string(pin) + ")";

        ctx.add_detected_component(comp, claimed);
    };

    ComponentRegistry::instance().register_component(def);
    return true;
}();
