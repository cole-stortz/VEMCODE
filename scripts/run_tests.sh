#!/bin/bash
# Runs every test sketch in app/sketches/tests/ for up to 5s each via the
# headless CLI and reports pass/fail. "Pass" = ran without crashing (most
# sketches loop forever on purpose, so a timeout+SIGINT exit, code 124, is
# expected, not a failure). Smoke test only -- no expected-output fixtures
# to diff against yet (see ROADMAP.md's regression-suite item).

set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

TESTS_DIR="app/sketches/tests"
TIMEOUT_SECS=5

VEMCODE_BIN="./app/VEMCODE"
[ -f "${VEMCODE_BIN}.exe" ] && VEMCODE_BIN="${VEMCODE_BIN}.exe"

TIMEOUT_BIN="timeout"
[ -x "/usr/bin/timeout" ] && TIMEOUT_BIN="/usr/bin/timeout"

declare -a PASSED=()
declare -a FAILED=()

for dir in "$TESTS_DIR"/*/; do
    name=$(basename "$dir")
    sketch="$dir$name.cpp"
    [ -f "$sketch" ] || continue

    echo "=== $name ==="
    output=$("$TIMEOUT_BIN" -k 2 --signal=INT "$TIMEOUT_SECS" "$VEMCODE_BIN" "$sketch" 2>&1)
    code=$?
    echo "$output"
    echo

    if [ "$code" -eq 124 ] || [ "$code" -eq 0 ] || [ "$code" -eq 137 ]; then
        PASSED+=("$name")
    else
        FAILED+=("$name (exit $code)")
    fi
done

echo "================ SUMMARY ================"
echo "Passed (${#PASSED[@]}): ${PASSED[*]:-none}"
echo "Failed (${#FAILED[@]}): ${FAILED[*]:-none}"

[ ${#FAILED[@]} -eq 0 ]
