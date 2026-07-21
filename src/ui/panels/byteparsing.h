#pragma once
#include <QString>
#include <cstdint>
#include <vector>

// Parses a comma-separated list of ints (accepts "0x.." hex or decimal, per
// QString::toInt's base-0 auto-detect) into bytes, truncated to the low 8
// bits. Used by the virtual I2C/SPI device panels to read their
// response-byte edit fields.
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
