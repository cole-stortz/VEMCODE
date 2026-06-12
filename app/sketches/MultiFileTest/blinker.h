#pragma once

struct BlinkPattern {
    int on_ms;
    int off_ms;
};

int next_interval(const BlinkPattern& p, bool is_on);