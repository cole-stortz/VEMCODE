#pragma once
#include "src/core/host/sketchhost.h"
#include <QThread>
#include <QString>
#include <atomic>
#include <QMutex>
#include <vector>
#include <utility>

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
    void injectKeypadWiring(const std::vector<int>& col_pins, const std::vector<int>& row_pins);
    void injectKeypadPress(int row_pin, int col_pin, bool pressed);
    void injectDht(int pin, float temp_c, float humidity);
    void injectWireDevice(int address, const std::vector<uint8_t>& bytes);
    void injectSpiBytes(const std::vector<uint8_t>& bytes);
    void setProfile(BoardProfile p);
    void injectSoftSerial(int rxPin, const QString& data);
    void resetRuntimeState();

    // name -> type string ("int"/"float"/"long"/"ulong"/"bool"), polled from
    // the sketch thread's own loop so reads never race the sketch's writes.
    void setWatchList(std::vector<std::pair<QString, QString>> vars);

signals:
    void serialOutput(QString text);
    void serial1Output(QString text);
    void serial2Output(QString text);
    void softSerialOutput(int rxPin, QString text);
    void pinChanged(int pin, int value);
    void sketchReloaded();
    void loadFailed(QString reason);
    void sketchCrashed(QString reason);
    void variableChanged(QString name, QString value);
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
    QMutex        watch_mutex_;
    std::vector<std::pair<QString, QString>> watchList_;
};