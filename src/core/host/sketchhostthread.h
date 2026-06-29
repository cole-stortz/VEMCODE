#pragma once
#include "src/core/host/sketchhost.h"
#include <QThread>
#include <QString>
#include <atomic>
#include <QMutex>

// Runs SketchHost on a background thread so the simulation never blocks the UI.
class SketchThread : public QThread {
    Q_OBJECT

public:
    explicit SketchThread(QObject* parent = nullptr);

    void startSketch(const QString& dll_path);
    void stopSketch();

    void injectPin(int pin, int value);
    void injectButtonBounce(int pin, int value);
    void setAnalogNoise(bool enabled);

    void injectAnalog(int pin, int value);

    void setSpeed(float speed);

    void injectSerial(const QString& data);

    void injectPulseDuration(int pin, unsigned long micros);
    void injectColor(int out_pin, int s2_pin, int s3_pin, int r, int g, int b);
    void setProfile(BoardProfile p);
    void injectSoftSerial(int rxPin, const QString& data);

signals:
    void serialOutput(QString text);
    void serial1Output(QString text);
    void serial2Output(QString text);
    void softSerialOutput(int rxPin, QString text);
    void pinChanged(int pin, int value);
    void sketchReloaded();
    void loadFailed(QString reason);
    void sketchCrashed(QString reason);
    void variableChanged(QString name, int value);
    void lcdPrint(int pin, int row, QString text);
    void watchdogReset();
    void sleepChanged(bool sleeping);

protected:
    void run() override;

private:
    SketchHost    host_;
    QString       dll_path_;
    std::atomic<bool> running_{false};
    QMutex        inject_mutex_;    
};