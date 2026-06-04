#include "src/core/runtime/arduinoapi.h"
#include "src/core/host/sketchhost.h"
#include <iostream>

int main() {
    std::cout << "=== VEMCODE Core Test ===\n";

    SketchHost host;

    if (!host.load("sketch.dll")) {
        std::cerr << "Failed to load sketch.dll\n";
        return 1;
    }

    int loop_count = 0;
    while (true) {
        host.run_loop();
        loop_count++;

        if (loop_count % 5 == 0)
            host.reload_if_changed();
    }
}