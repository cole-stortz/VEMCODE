#ifndef VB_KEYPAD_H
#define VB_KEYPAD_H
// makeKeymap matches the real Keypad.h macro exactly -- it just hands back a
// char* view of the user's own keys[ROWS][COLS] array, no copy needed.
#define makeKeymap(x) ((char*)(x))
#define NO_KEY ((char)0)

class Keypad {
public:
    // Real starter-kit sketches declare Keypad as a global (Keypad keypad =
    // Keypad(...);), so this constructor runs during static init -- before
    // vb_init() sets `api`. It must not touch api-> at all; pin setup happens
    // lazily on the first getKey() call instead.
    Keypad(char* keymap, uint8_t* rowPins, uint8_t* colPins, uint8_t rows, uint8_t cols)
        : keymap_(keymap), rows_(rows > 8 ? 8 : rows), cols_(cols > 8 ? 8 : cols) {
        for (uint8_t r = 0; r < rows_; ++r) rowPins_[r] = rowPins[r];
        for (uint8_t c = 0; c < cols_; ++c) colPins_[c] = colPins[c];
    }

    // Faithful row/col scan -- real electrical simulation, not a shortcut:
    // drive one row low at a time and read the columns, exactly like real
    // Keypad.h. The host resolves each digitalRead(col) from which row is
    // currently driven low plus which (row,col) key the canvas has pressed.
    char getKey() {
        if (!initialized_) {
            for (uint8_t r = 0; r < rows_; ++r) {
                api->pinMode(rowPins_[r], 1 /*OUTPUT*/);
                api->digitalWrite(rowPins_[r], 1);
            }
            for (uint8_t c = 0; c < cols_; ++c)
                api->pinMode(colPins_[c], 2 /*INPUT_PULLUP*/);
            initialized_ = true;
        }
        for (uint8_t r = 0; r < rows_; ++r) {
            api->digitalWrite(rowPins_[r], 0);
            for (uint8_t c = 0; c < cols_; ++c) {
                if (api->digitalRead(colPins_[c]) == 0) {
                    api->digitalWrite(rowPins_[r], 1);
                    return keymap_[r * cols_ + c];
                }
            }
            api->digitalWrite(rowPins_[r], 1);
        }
        return NO_KEY;
    }

private:
    char* keymap_;
    uint8_t rowPins_[8];
    uint8_t colPins_[8];
    uint8_t rows_, cols_;
    bool initialized_ = false;
};
#endif
