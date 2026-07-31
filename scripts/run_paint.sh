#!/bin/bash
# Builds (first run only) and launches the standalone paint preview tool:
#   ./scripts/run_paint.sh
# Edit tools/paint_preview/main.cpp's PASTE ZONE, rerun, see the change.

set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

[ -f tools/paint_preview/build/CMakeCache.txt ] || cmake -B tools/paint_preview/build -S tools/paint_preview || exit 1
cmake --build tools/paint_preview/build -j"$(nproc)" || exit 1

./tools/paint_preview/build/paint_preview
