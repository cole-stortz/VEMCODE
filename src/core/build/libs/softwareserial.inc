#ifndef VB_SOFTWARESERIAL_H
#define VB_SOFTWARESERIAL_H
class SoftwareSerial {
public:
    SoftwareSerial(int rxPin, int txPin) : rxPin_(rxPin), txPin_(txPin) {}
    void begin(int baud) { if (api) api->soft_serial_begin(rxPin_, baud); }
    void print(const char* s)   { if (api) api->soft_serial_print(rxPin_, s ? s : ""); }
    void print(const String& s) { if (api) api->soft_serial_print(rxPin_, s.c_str()); }
    void print(int v)           { if (api) { auto s = std::to_string(v); api->soft_serial_print(rxPin_, s.c_str()); } }
    void print(long v)          { if (api) { auto s = std::to_string(v); api->soft_serial_print(rxPin_, s.c_str()); } }
    void print(unsigned long v) { if (api) { auto s = std::to_string(v); api->soft_serial_print(rxPin_, s.c_str()); } }
    void print(float v)         { if (api) { auto s = std::to_string(v); api->soft_serial_print(rxPin_, s.c_str()); } }
    void println(const char* s)   { if (api) api->soft_serial_println(rxPin_, s ? s : ""); }
    void println(const String& s) { if (api) api->soft_serial_println(rxPin_, s.c_str()); }
    void println(int v)           { if (api) { auto s = std::to_string(v); api->soft_serial_println(rxPin_, s.c_str()); } }
    void println(long v)          { if (api) { auto s = std::to_string(v); api->soft_serial_println(rxPin_, s.c_str()); } }
    void println(unsigned long v) { if (api) { auto s = std::to_string(v); api->soft_serial_println(rxPin_, s.c_str()); } }
    void println(float v)         { if (api) { auto s = std::to_string(v); api->soft_serial_println(rxPin_, s.c_str()); } }
    int available() { return api ? api->soft_serial_available(rxPin_) : 0; }
    int read() { return api ? api->soft_serial_read(rxPin_) : -1; }
    int peek() { return api ? api->soft_serial_peek(rxPin_) : -1; }
    void write(uint8_t b) { if (api) { char s[2] = {(char)b, '\0'}; api->soft_serial_print(rxPin_, s); } }
    void write(const uint8_t* buf, size_t n) { for (size_t i = 0; i < n; ++i) write(buf[i]); }
    void println() { if (api) api->soft_serial_println(rxPin_, ""); }
    bool listen() { return true; }
    bool isListening() { return true; }
    bool overflow() { return false; }
private:
    int rxPin_, txPin_;
};
#endif
