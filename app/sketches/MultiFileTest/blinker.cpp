#include "blinker.h"

int next_interval(const BlinkPattern& p, bool is_on) {
    return is_on ? p.on_ms : p.off_ms;
}