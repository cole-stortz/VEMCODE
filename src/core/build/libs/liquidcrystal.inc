#ifndef VB_LIQUIDCRYSTAL_H
#define VB_LIQUIDCRYSTAL_H
#include <string>
#include <cstring>
class LiquidCrystal {
public:
    LiquidCrystal(int rs, int, int, int, int, int) : rs_(rs), row_(0), col_(0) {
        memset(buf_, ' ', sizeof(buf_)); buf_[0][16]=buf_[1][16]='\0'; }
    void begin(int, int) { if (api) { api->digitalWrite(rs_, 1); flush_(0); flush_(1); } }
    void clear() {
        memset(buf_, ' ', sizeof(buf_)); buf_[0][16]=buf_[1][16]='\0';
        row_=0; col_=0;
        if (api) { flush_(0); flush_(1); } }
    void setCursor(int col, int row) {
        col_ = col < 0 ? 0 : (col < 16 ? col : 15);
        row_ = row < 0 ? 0 : (row < 2 ? row : 1);
    }
    void write(uint8_t c) {
        if (col_ >= 16) return;
        buf_[row_][col_++] = (c < 32) ? '*' : (char)c;
        if (api) flush_(row_); }
    void write(char c) { write((uint8_t)c); }
    void print(const char* s) { if (!s) return; while (*s) write((uint8_t)*s++); }
    void print(const String& s) { print(s.c_str()); }
    void print(int v)           { auto s=std::to_string(v); print(s.c_str()); }
    void print(long v)          { auto s=std::to_string(v); print(s.c_str()); }
    void print(unsigned long v) { auto s=std::to_string(v); print(s.c_str()); }
    void print(float v)         { auto s=std::to_string(v); print(s.c_str()); }
    void createChar(uint8_t, uint8_t*) {}
private:
    int rs_, row_, col_;
    char buf_[2][17];
    void flush_(int row) { if (api) api->lcd_print(rs_, row, buf_[row]); }
};
#endif
