#!/bin/bash
# Runs every example sketch in app/sketches/examples/ via the headless CLI
# and reports pass/fail. Unlike run_tests.sh, these have no .timeline
# fixtures or ASSERTs -- "pass" just means it compiled and ran without
# crashing, most of them loop forever on purpose, so a timeout+SIGINT exit
# (code 124, or 137 if it had to be killed) is expected, not a failure.

set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

cmake --build build

EXAMPLES_DIR="app/sketches/examples"
TIMEOUT_SECS=1

VEMCODE_BIN="./app/VEMCODE"
[ -f "$VEMCODE_BIN" ] || VEMCODE_BIN="${VEMCODE_BIN}.exe"

TIMEOUT_BIN="timeout"
[ -x "/usr/bin/timeout" ] && TIMEOUT_BIN="/usr/bin/timeout"

declare -a PASSED=()
declare -a FAILED=()

for dir in "$EXAMPLES_DIR"/*/; do
    name=$(basename "$dir")
    sketch="$dir$name.cpp"
    [ -f "$sketch" ] || continue

    # Buffered rather than streamed live (unlike run_tests.sh) -- there's no
    # per-sketch ASSERT output worth watching as it happens here, so only
    # show it when a sketch actually fails.
    out=$("$TIMEOUT_BIN" -k 1 "$TIMEOUT_SECS" "$VEMCODE_BIN" "$sketch" timeout=2 speed=10 2>&1)
    code=$?

    if [ "$code" -eq 0 ] || [ "$code" -eq 124 ] || [ "$code" -eq 137 ]; then
        echo "ok: $name"
        PASSED+=("$name")
    else
        echo "!!!! FAIL ($code): $name"
        echo "$out" | tail -6
        FAILED+=("$name (exit $code)")
    fi
done

echo "================ SUMMARY ================"
echo "Passed (${#PASSED[@]}): ${PASSED[*]:-none}"
echo "Failed (${#FAILED[@]}): ${FAILED[*]:-none}"

[ ${#FAILED[@]} -eq 0 ]
