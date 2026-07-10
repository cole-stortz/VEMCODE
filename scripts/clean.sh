#!/bin/bash
# Removes generated per-sketch build artifacts under app/sketches/ (compiled
# .so/.dll/.dylib, their .tmp copies, and _vb_temp.cpp) -- same patterns
# .gitignore excludes. Always safe to delete; regenerated on the next run.

set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

PATTERNS=(
    "*.so" "*.tmp.so"
    "*.dll" "*.tmp.dll"
    "*.dylib" "*.tmp.dylib"
    "_vb_temp.cpp"
)

count=0
for pattern in "${PATTERNS[@]}"; do
    while IFS= read -r -d '' f; do
        rm -f -- "$f"
        count=$((count + 1))
    done < <(find app/sketches -name "$pattern" -type f -print0)
done

echo "Removed $count generated file(s) under app/sketches/"
