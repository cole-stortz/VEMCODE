#ifndef VB_LIQUIDCRYSTAL_I2C_H
#define VB_LIQUIDCRYSTAL_I2C_H
#include <string>
#include <cstring>
class LiquidCrystal_I2C {
public:
    LiquidCrystal_I2C(uint8_t addr, uint8_t cols = 16, uint8_t rows = 2)
        : cols_(cols > 20 ? 20 : cols), rows_(rows > 4 ? 4 : rows),
          pinKey_(g_next_i2c_bus_pin++) {
        (void)addr;
        memset(buf_, ' ', sizeof(buf_));
        for (int r = 0; r < 4; ++r) buf_[r][20] = '\0';
    }
    void init() {}
    void begin(int c = -1, int r = -1) {
        if (c > 0) cols_ = c > 20 ? 20 : c;
        if (r > 0) rows_ = r > 4 ? 4 : r;
        clear();
    }
    void backlight() {}
    void noBacklight() {}
    void clear() {
        memset(buf_, ' ', sizeof(buf_));
        for (int r = 0; r < 4; ++r) buf_[r][20] = '\0';
        row_ = 0; col_ = 0;
        if (api) for (int r = 0; r < rows_; ++r) flush_(r);
    }
    void setCursor(int col, int row) {
        col_ = col < 0 ? 0 : (col < cols_ ? col : cols_ - 1);
        row_ = row < 0 ? 0 : (row < rows_ ? row : rows_ - 1);
    }
    void write(uint8_t c) {
        if (col_ >= cols_) return;
        buf_[row_][col_++] = (c < 32) ? '*' : (char)c;
        if (api) flush_(row_);
    }
    void write(char c) { write((uint8_t)c); }
    void print(const char* s) { if (!s) return; while (*s) write((uint8_t)*s++); }
    void print(const String& s) { print(s.c_str()); }
    void print(int v)           { auto s=std::to_string(v); print(s.c_str()); }
    void print(long v)          { auto s=std::to_string(v); print(s.c_str()); }
    void print(unsigned long v) { auto s=std::to_string(v); print(s.c_str()); }
    void print(float v)         { auto s=std::to_string(v); print(s.c_str()); }
    void createChar(uint8_t, uint8_t*) {}
private:
    int cols_, rows_, pinKey_;
    int row_ = 0, col_ = 0;
    char buf_[4][21];
    void flush_(int row) {
        if (!api) return;
        char tmp[21];
        memcpy(tmp, buf_[row], cols_);
        tmp[cols_] = '\0';
        api->lcd_print(pinKey_, row, tmp);
    }
};
#endif
