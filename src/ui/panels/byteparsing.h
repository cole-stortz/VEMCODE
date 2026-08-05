#pragma once
#include <QString>
#include <cstdint>
#include <vector>

// Parses a comma-separated list of ints (0x.. hex or decimal, base-0 auto-detect) into
// bytes truncated to the low 8 bits; used by the virtual I2C/SPI panels' response fields.
inline std::vector<uint8_t> parseByteList(const QString& text) {
    std::vector<uint8_t> bytes;
    for (const QString& tok : text.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        int v = tok.trimmed().toInt(&ok, 0);
        if (ok)
            bytes.push_back((uint8_t)(v & 0xFF));
    }
    return bytes;
}
