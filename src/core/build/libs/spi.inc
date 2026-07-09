// SPI -- no per-device addressing (real SPI selects via a plain digitalWrite
// CS pin); transfer() cycles through a configurable response sequence.
#define MSBFIRST 1
#define LSBFIRST 0
#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C
struct SPISettings {
    SPISettings() {}
    SPISettings(unsigned long /*clock*/, uint8_t /*bitOrder*/, uint8_t /*dataMode*/) {}
};
inline void spi_begin() {}
inline void spi_begin_transaction(const SPISettings& /*settings*/) {}
inline void spi_end_transaction() {}
inline uint8_t spi_transfer(uint8_t b) { return api->spi_transfer(b); }
inline void spi_transfer(void* buf, size_t count) {
    uint8_t* p = (uint8_t*)buf;
    for (size_t i = 0; i < count; ++i) p[i] = api->spi_transfer(p[i]);
}
