#!/bin/bash
# Builds VEMCODE then launches it, forwarding all arguments to ./app/VEMCODE:
#   ./scripts/run.sh                  -> build, launch the GUI
#   ./scripts/run.sh SomeSketch.cpp   -> build, run that sketch headlessly
# Assumes `cmake -B build -S .` has already been run once.

set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

cmake --build build -j"$(nproc)" || exit 1

./app/VEMCODE "$@"
