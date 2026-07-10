// Wire's optional args and overloaded write() don't fit a bare ArduinoAPI
// function pointer, so these wrap the fixed-signature api-> calls instead.
inline void wire_begin() {}
inline void wire_begin(int /*slaveAddress*/) {}
inline size_t wire_write(uint8_t b) { api->wire_write(b); return 1; }
inline size_t wire_write(const char* s) {
    size_t n = 0;
    while (s && *s) { api->wire_write((uint8_t)*s++); ++n; }
    return n;
}
inline size_t wire_write(const uint8_t* buf, size_t n) {
    for (size_t i = 0; i < n; ++i) api->wire_write(buf[i]);
    return n;
}
// An int literal like Wire.write(0x00) converts equally well to uint8_t or to
// a null const char* , which is ambiguous without an exact-match overload to
// win the resolution -- same fix real Arduino's Print::write() uses.
inline size_t wire_write(int n)           { return wire_write((uint8_t)n); }
inline size_t wire_write(unsigned int n)  { return wire_write((uint8_t)n); }
inline size_t wire_write(long n)          { return wire_write((uint8_t)n); }
inline size_t wire_write(unsigned long n) { return wire_write((uint8_t)n); }
inline uint8_t wire_end_transmission(bool /*stopBit*/ = true) {
    return (uint8_t)api->wire_end_transmission();
}
inline uint8_t wire_request_from(int address, int quantity, bool /*stopBit*/ = true) {
    return (uint8_t)api->wire_request_from(address, quantity);
}
