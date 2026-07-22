#ifndef VB_DHT_H
#define VB_DHT_H
// Sensor-type selectors -- only used to pick a constructor overload in real
// sketches; the simulated reading doesn't depend on which one is used.
#define DHT11  11
#define DHT21  21
#define DHT22  22
#define AM2301 21

class DHT {
public:
    // Real starter-kit sketches declare DHT as a global (DHT dht(DHTPIN,
    // DHTTYPE);), so this constructor runs before vb_init() sets `api` --
    // must not touch api-> here (same reason as the Keypad class).
    DHT(uint8_t pin, uint8_t type = DHT22) : pin_(pin) { (void)type; }

    void begin() {}

    float readTemperature(bool fahrenheit = false, bool /*force*/ = false) {
        float c = api->dht_read_temperature(pin_);
        return fahrenheit ? c * 9.0f / 5.0f + 32.0f : c;
    }

    float readHumidity(bool /*force*/ = false) {
        return api->dht_read_humidity(pin_);
    }

private:
    uint8_t pin_;
};
#endif
