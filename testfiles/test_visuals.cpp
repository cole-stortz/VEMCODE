#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QtMath>

// -------------------------------------------------------
// Standalone visual test for component shapes.
// Run this to iterate on appearance without touching the main app.
// Build target: test_visuals (see CMakeLists.txt)
// -------------------------------------------------------

static const QColor MOTOR_OFF    ("#1f351f");
static const QColor MOTOR_ON     ("#4f7f4f");
static const QColor MOTOR_HAND   ("#44aaff");
static const QColor MOTOR_BRAKE  ("#ff4444");
static const QColor MOTOR_STOP   ("#444444");

static const QColor SERVO_OFF    ("#001a3a");
static const QColor SERVO_ON     ("#44aaff");
static const QColor SERVO_ARM    ("#8900bb");

static const QColor LED_OFF      ("#3a3000");
static const QColor LED_ON       ("#ffdd44");

static const QColor BUTTON_OFF   ("#003a15");
static const QColor BUTTON_ON    ("#44ff88");

static const QColor SWITCH_OFF   ("#003a15");
static const QColor SWITCH_ON    ("#44ff88");

static const QColor BUZZER_OFF   ("#3a1a00");
static const QColor BUZZER_ON    ("#ff8844");

static const QColor DIST_PCB     ("#0d1a0d");
static const QColor DIST_BORDER  ("#2a4a2a");
static const QColor DIST_LENS    ("#1a2a3a");
static const QColor DIST_RING    ("#2a4a6a");

static const QColor POT_OFF      ("#1a1a3a");
static const QColor POT_ON       ("#44ffcc");

static const QColor CSENSOR_OFF  ("#1a0040");
static const QColor CSENSOR_ON   ("#aa44ff");

static const QColor LEADS        ("#888888");
static const QColor LABEL        ("#cccccc");
static const QColor SUBLABEL     ("#666666");

// -------------------------------------------------------

static QColor motorHandColor(bool cw, bool acw) {
    if (cw && acw) return MOTOR_BRAKE;
    if (cw || acw) return MOTOR_HAND;
    return         MOTOR_STOP;
}

// -------------------------------------------------------

class ShapeTest : public QWidget {
public:
    explicit ShapeTest(QWidget* parent = nullptr) : QWidget(parent) {
        setWindowTitle("Component Visuals Test");
        resize(940, 780);
        setStyleSheet("background: #1a1a1a;");
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        drawSectionLabel(p, 30, 20, "MOTOR");
        drawMotor(p, 80,  75, 0,   false, false);
        drawMotor(p, 200, 75, 60,  true,  false);
        drawMotor(p, 320, 75, -60, false, true);
        drawMotor(p, 440, 75, 0,   true,  true);

        drawSectionLabel(p, 30, 175, "SERVO");
        drawServo(p, 50,  210, 0);
        drawServo(p, 185, 210, 45);
        drawServo(p, 320, 210, 90);
        drawServo(p, 455, 210, 135);
        drawServo(p, 590, 210, 180);

        drawSectionLabel(p, 30, 325, "LED");
        drawLED(p, 75,  370, false);
        drawLED(p, 150, 370, true);

        drawSectionLabel(p, 230, 325, "BUTTON");
        drawButton(p, 275, 370, false);
        drawButton(p, 360, 370, true);

        drawSectionLabel(p, 460, 325, "SWITCH");
        drawSwitch(p, 460, 345, false);
        drawSwitch(p, 560, 345, true);

        drawSectionLabel(p, 690, 325, "BUZZER");
        drawBuzzer(p, 730, 370, false);
        drawBuzzer(p, 820, 370, true);

        drawSectionLabel(p, 30, 480, "DISTANCE");
        drawDistanceSensor(p, 30, 505);

        drawSectionLabel(p, 260, 480, "POTENTIOMETER");
        drawPotentiometer(p, 310, 540, 0);
        drawPotentiometer(p, 420, 540, 512);
        drawPotentiometer(p, 530, 540, 1023);

        drawSectionLabel(p, 30, 630, "COLOR SENSOR");
        drawColorSensor(p, 90,  685, 0,   0,   0);    // idle
        drawColorSensor(p, 210, 685, 255, 0,   0);    // red
        drawColorSensor(p, 330, 685, 0,   255, 0);    // green
        drawColorSensor(p, 450, 685, 0,   0,   255);  // blue
        drawColorSensor(p, 570, 685, 180, 60,  220);  // mixed
    }

private:
    void drawSectionLabel(QPainter& p, int x, int y, const char* text) {
        p.setPen(SUBLABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(x, y, text);
    }

    void drawLeads(QPainter& p, int x1, int y1, int x2, int y2, int len) {
        p.setPen(QPen(LEADS, 2));
        p.drawLine(x1, y1, x1, y1 + len);
        p.drawLine(x2, y2, x2, y2 + len);
    }

    // Motor: clock-face circle with a rotating hand.
    // angle_deg: 0 = 12 o'clock, positive = clockwise
    void drawMotor(QPainter& p, int cx, int cy, int angle_deg, bool cw, bool acw) {
        const int R = 40;
        bool active = cw || acw;

        QColor bg = active ? MOTOR_ON : MOTOR_OFF;
        p.setPen(QPen(bg.lighter(160), 2));
        p.setBrush(bg);
        p.drawEllipse(cx - R, cy - R, R * 2, R * 2);

        QColor hand = motorHandColor(cw, acw);
        float rad = qDegreesToRadians((float)angle_deg - 90.0f);
        int hx = cx + (int)(qCos(rad) * (R - 3));
        int hy = cy + (int)(qSin(rad) * (R - 3));
        p.setPen(QPen(hand, 5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(cx, cy, hx, hy);

        p.setPen(Qt::NoPen);
        p.setBrush(bg.lighter(160));
        p.drawEllipse(cx - 4, cy - 4, 8, 8);

        QString state = (cw && acw) ? "BRAKE" : cw ? "CW" : acw ? "CCW" : "STOP";
        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(cx - 20, cy + R + 18, state);
    }

    // Servo: rectangular body with an arm pivoting from the top center.
    // angle_deg: 0 = arm pointing left, 90 = straight up, 180 = pointing right
    void drawServo(QPainter& p, int x, int y, int angle_deg) {
        const int BW = 80, BH = 40;
        const int ARM = 40;

        p.setPen(QPen(SERVO_ON.darker(160), 2));
        p.setBrush(SERVO_OFF);
        p.drawRoundedRect(x, y, BW, BH, 5, 5);

        int px = x + BW / 2;
        int py = y;

        float rad = qDegreesToRadians(180.0f - (float)angle_deg);
        int ax = px + (int)(qCos(rad) * ARM);
        int ay = py + (int)(qSin(rad) * ARM);

        p.setPen(QPen(SERVO_ARM, 5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(px, py, ax, ay);

        p.setPen(QPen(SERVO_ON, 1));
        p.setBrush(SERVO_ON);
        p.drawEllipse(px - 5, py - 5, 10, 10);

        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(x + BW / 2 - 14, y + BH + 18, QString("%1°").arg(angle_deg));
    }

    // LED: circle dome with two leads at the bottom
    void drawLED(QPainter& p, int cx, int cy, bool active) {
        const int R = 20;
        const int LEAD = 22;

        drawLeads(p, cx - 7, cy + R, cx + 7, cy + R, LEAD);

        QColor bg = active ? LED_ON : LED_OFF;
        p.setPen(QPen(bg.lighter(160), 2));
        p.setBrush(bg);
        p.drawEllipse(cx - R, cy - R, R * 2, R * 2);

        // Flat cathode edge at base of dome
        p.setPen(QPen(bg.lighter(180), 1));
        p.drawLine(cx - R + 4, cy + R - 5, cx + R - 4, cy + R - 5);

        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(cx - 12, cy + R + LEAD + 14, active ? "HIGH" : "LOW");
    }

    // Button: square body with a raised circular actuator, two leads
    void drawButton(QPainter& p, int cx, int cy, bool pressed) {
        const int BW = 44, BH = 34;
        const int LEAD = 18;
        const int CAP_R = pressed ? 7 : 10;
        const int CAP_OFFSET = pressed ? 4 : 0;

        drawLeads(p, cx - 9, cy + BH / 2, cx + 9, cy + BH / 2, LEAD);

        QColor bg = pressed ? BUTTON_ON : BUTTON_OFF;
        p.setPen(QPen(bg.lighter(160), 2));
        p.setBrush(bg);
        p.drawRoundedRect(cx - BW / 2, cy - BH / 2, BW, BH, 3, 3);

        // Actuator cap (sinks when pressed)
        p.setPen(QPen(bg.lighter(200), 1));
        p.setBrush(bg.lighter(140));
        p.drawEllipse(cx - CAP_R, cy - BH / 2 - CAP_R + CAP_OFFSET, CAP_R * 2, CAP_R * 2);

        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(cx - 16, cy + BH / 2 + LEAD + 14, pressed ? "PRESSED" : "UP");
    }

    // Switch: outer rectangle, inner square slides left (off) or right (on)
    void drawSwitch(QPainter& p, int x, int y, bool on) {
        const int BW = 64, BH = 28;
        const int IND = 20;
        const int LEAD = 18;

        drawLeads(p, x + BW / 3, y + BH, x + 2 * BW / 3, y + BH, LEAD);

        QColor bg = on ? SWITCH_ON : SWITCH_OFF;
        p.setPen(QPen(bg.lighter(150), 2));
        p.setBrush(bg.darker(180));
        p.drawRoundedRect(x, y, BW, BH, 4, 4);

        // Sliding indicator
        int ind_x = on ? x + BW - IND - 4 : x + 4;
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(ind_x, y + (BH - IND) / 2, IND, IND, 3, 3);

        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(x + BW / 2 - 8, y + BH + LEAD + 14, on ? "ON" : "OFF");
    }

    // Buzzer: circle with concentric inner rings to suggest a piezo disc
    void drawBuzzer(QPainter& p, int cx, int cy, bool active) {
        const int R = 22;
        const int LEAD = 22;

        drawLeads(p, cx - 7, cy + R, cx + 7, cy + R, LEAD);

        QColor bg = active ? BUZZER_ON : BUZZER_OFF;
        p.setPen(QPen(bg.lighter(160), 2));
        p.setBrush(bg);
        p.drawEllipse(cx - R, cy - R, R * 2, R * 2);

        // Concentric rings (piezo disc detail)
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(bg.lighter(200), 1));
        p.drawEllipse(cx - R / 2, cy - R / 2, R, R);
        p.drawEllipse(cx - R / 4, cy - R / 4, R / 2, R / 2);

        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(cx - 12, cy + R + LEAD + 14, active ? "ON" : "OFF");
    }

    // Distance sensor: PCB rectangle with two ultrasonic transducer circles
    void drawDistanceSensor(QPainter& p, int x, int y) {
        const int BW = 130, BH = 52;
        const int TR = 18;   // transducer radius
        const int LEAD = 16;

        // PCB body
        p.setPen(QPen(DIST_BORDER, 2));
        p.setBrush(DIST_PCB);
        p.drawRoundedRect(x, y, BW, BH, 4, 4);

        // Two transducer circles, centered vertically, side by side
        int cy = y + BH / 2;
        int c1x = x + BW / 2 - TR - 10;
        int c2x = x + BW / 2 + TR + 10;

        for (int cx : {c1x, c2x}) {
            // Outer ring
            p.setPen(QPen(DIST_RING, 2));
            p.setBrush(DIST_LENS);
            p.drawEllipse(cx - TR, cy - TR, TR * 2, TR * 2);
            // Inner detail ring
            p.setPen(QPen(DIST_RING.lighter(130), 1));
            p.setBrush(DIST_LENS.lighter(120));
            p.drawEllipse(cx - TR / 2, cy - TR / 2, TR, TR);
        }

        // 4 pin leads at the bottom
        p.setPen(QPen(LEADS, 2));
        for (int i = 0; i < 4; i++) {
            int lx = x + 16 + i * (BW - 32) / 3;
            p.drawLine(lx, y + BH, lx, y + BH + LEAD);
        }

        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(x + BW / 2 - 30, y + BH + LEAD + 14, "HC-SR04");
    }

    // Color sensor: square PCB with a circular lens; lens fills with the detected R/G/B color
    void drawColorSensor(QPainter& p, int cx, int cy, int r, int g, int b) {
        const int BW = 60, BH = 60;
        const int LENS_R = 18;
        const int LEAD = 16;
        bool active = (r > 0 || g > 0 || b > 0);

        // PCB body
        p.setPen(QPen(CSENSOR_ON.darker(150), 2));
        p.setBrush(CSENSOR_OFF);
        p.drawRoundedRect(cx - BW / 2, cy - BH / 2, BW, BH, 4, 4);

        // Lens circle -- fills with actual color so you can see what it's sensing
        QColor lens = active ? QColor(r, g, b) : CSENSOR_OFF.lighter(130);
        p.setPen(QPen(CSENSOR_ON.darker(120), 1));
        p.setBrush(lens);
        p.drawEllipse(cx - LENS_R, cy - LENS_R, LENS_R * 2, LENS_R * 2);

        // 5 pin leads (S0, S1, S2, S3, OUT)
        p.setPen(QPen(LEADS, 2));
        for (int i = 0; i < 5; i++) {
            int lx = cx - BW / 2 + 6 + i * (BW - 12) / 4;
            p.drawLine(lx, cy + BH / 2, lx, cy + BH / 2 + LEAD);
        }

        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        QString lbl = active ? QString("R%1G%2B%3").arg(r).arg(g).arg(b) : "idle";
        p.drawText(cx - 24, cy + BH / 2 + LEAD + 14, lbl);
    }

    // Potentiometer: circular dial with a 270° sweep indicator
    void drawPotentiometer(QPainter& p, int cx, int cy, int value) {
        const int R = 28;
        const int LEAD = 20;

        drawLeads(p, cx - 9, cy + R, cx + 9, cy + R, LEAD);

        p.setPen(QPen(POT_OFF.lighter(160), 2));
        p.setBrush(POT_OFF);
        p.drawEllipse(cx - R, cy - R, R * 2, R * 2);

        // Dial indicator -- sweeps 270° from 7 o'clock (0) to 5 o'clock (1023)
        float angle_deg = 225.0f - (value / 1023.0f) * 270.0f;
        float rad = qDegreesToRadians(angle_deg);
        int hx = cx + (int)(qCos(rad) * (R - 8));
        int hy = cy - (int)(qSin(rad) * (R - 8));
        p.setPen(QPen(POT_ON, 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(cx, cy, hx, hy);

        p.setPen(Qt::NoPen);
        p.setBrush(POT_ON);
        p.drawEllipse(cx - 3, cy - 3, 6, 6);

        p.setPen(LABEL);
        p.setFont(QFont("Courier New", 9));
        p.drawText(cx - 16, cy + R + LEAD + 14, QString::number(value));
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ShapeTest w;
    w.show();
    return app.exec();
}
