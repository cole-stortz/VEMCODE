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
        for (int a = 0; a < MAX_DEVICES; ++a)
            for (int r = 0; r < 8; ++r) rows_[a][r] = 0;
    }

    void setLed(int addr, int row, int col, bool state) {
        if (addr < 0 || addr >= MAX_DEVICES || row < 0 || row > 7 || col < 0 || col > 7) return;
        uint8_t mask = 1 << (7 - col);
        if (state) rows_[addr][row] |= mask; else rows_[addr][row] &= (uint8_t)~mask;
        flush_(addr, row);
    }

    void setRow(int addr, int row, uint8_t value) {
        if (addr < 0 || addr >= MAX_DEVICES || row < 0 || row > 7) return;
        rows_[addr][row] = value;
        flush_(addr, row);
    }

    void setColumn(int addr, int col, uint8_t value) {
        if (addr < 0 || addr >= MAX_DEVICES || col < 0 || col > 7) return;
        uint8_t mask = 1 << (7 - col);
        for (int r = 0; r < 8; ++r) {
            if (value & (1 << r)) rows_[addr][r] |= mask; else rows_[addr][r] &= (uint8_t)~mask;
            flush_(addr, r);
        }
    }

    void clearDisplay(int addr) {
        if (addr < 0 || addr >= MAX_DEVICES) return;
        for (int r = 0; r < 8; ++r) { rows_[addr][r] = 0; flush_(addr, r); }
    }

    // Real MAX7219 brightness/shutdown -- no visual equivalent in the sim, no-ops.
    void setIntensity(int addr, int intensity) {}
    void shutdown(int addr, bool status) {}

private:
    static constexpr int MAX_DEVICES = 8;
    int dinPin_, clkPin_, csPin_;
    uint8_t rows_[MAX_DEVICES][8];
    void flush_(int addr, int row) {
        if (api) api->matrix_set_row(csPin_, addr, row, rows_[addr][row]);
    }
};
#endif
