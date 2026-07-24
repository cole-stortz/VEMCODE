#ifndef VB_LEDCONTROL_H
#define VB_LEDCONTROL_H

class LedControl {
public:
    // Real starter-kit sketches declare LedControl as a global (LedControl lc =
    // LedControl(...);), so this constructor runs during static init -- before
    // vb_init() sets `api`. It must not touch api-> at all, same constraint
    // Keypad's constructor has.
    LedControl(int dataPin, int clkPin, int csPin, int numDevices = 1)
        : dinPin_(dataPin), clkPin_(clkPin), csPin_(csPin) {
        for (int r = 0; r < 8; ++r) rows_[r] = 0;
    }

    void setLed(int addr, int row, int col, bool state) {
        if (row < 0 || row > 7 || col < 0 || col > 7) return;
        uint8_t mask = 1 << (7 - col);
        if (state) rows_[row] |= mask; else rows_[row] &= (uint8_t)~mask;
        flush_(row);
    }

    void setRow(int addr, int row, uint8_t value) {
        if (row < 0 || row > 7) return;
        rows_[row] = value;
        flush_(row);
    }

    void setColumn(int addr, int col, uint8_t value) {
        if (col < 0 || col > 7) return;
        uint8_t mask = 1 << (7 - col);
        for (int r = 0; r < 8; ++r) {
            if (value & (1 << r)) rows_[r] |= mask; else rows_[r] &= (uint8_t)~mask;
            flush_(r);
        }
    }

    void clearDisplay(int addr) {
        for (int r = 0; r < 8; ++r) { rows_[r] = 0; flush_(r); }
    }

    // Real MAX7219 brightness/shutdown -- no visual equivalent in the sim, no-ops.
    void setIntensity(int addr, int intensity) {}
    void shutdown(int addr, bool status) {}

private:
    int dinPin_, clkPin_, csPin_;
    uint8_t rows_[8];
    void flush_(int row) { if (api) api->matrix_set_row(csPin_, row, rows_[row]); }
};
#endif
