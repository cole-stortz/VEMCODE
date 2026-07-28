#!/bin/bash
# Runs every test sketch in app/sketches/tests/ via the headless CLI and
# reports pass/fail.
#
# If a sketch has a sibling <name>.timeline fixture, it's run with
# timeline=true: the sketch's own ASSERT checks decide pass/fail (exit 0 =
# all assertions passed, matched below via the `run_headless`/TestRunner
# convention -- see src/core/host/timeline.h), and the run ends as soon as
# the timeline finishes rather than waiting out the full timeout. Any other
# exit code (including a timeout or crash) is a real failure for these.
#
# Sketches with no .timeline fixture fall back to the old smoke-test check:
# "pass" = ran without crashing. Most of those loop forever on purpose, so a
# timeout+SIGINT exit (code 124, or 137 if it had to be killed) is expected,
# not a failure.

set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

cmake --build build

TESTS_DIR="app/sketches/tests"
# Raised from the original 5s -- a couple of the timeline fixtures (real
# watchdog/sleep delays, several sequential loop() iterations) need more than
# that, though none need anywhere near this much; it's mainly headroom.
TIMEOUT_SECS=15

VEMCODE_BIN="./app/VEMCODE"
[ -f "$VEMCODE_BIN" ] || VEMCODE_BIN="${VEMCODE_BIN}.exe"

TIMEOUT_BIN="timeout"
[ -x "/usr/bin/timeout" ] && TIMEOUT_BIN="/usr/bin/timeout"

declare -a PASSED=()
declare -a FAILED=()

for dir in "$TESTS_DIR"/*/; do
    name=$(basename "$dir")
    sketch="$dir$name.cpp"
    timeline="$dir$name.timeline"
    [ -f "$sketch" ] || continue

    echo "=== $name ==="
    if [ -f "$timeline" ]; then
        # Buffered here (unlike the non-timeline branch below) -- a timeline
        # sketch's own Serial output is usually just debug noise around the
        # handful of PASS/FAIL/summary lines that actually matter, so capture
        # it and filter down to those on success. On failure, print
        # everything instead -- a compile error or crash needs the full
        # output to diagnose, not just the lines matching the filter.
        out=$("$TIMEOUT_BIN" -k 2 --signal=INT "$TIMEOUT_SECS" "$VEMCODE_BIN" "$sketch" timeline=true speed=10 2>&1)
        code=$?

        if [ "$code" -eq 0 ]; then
            echo "$out" | grep -E '^(PASS|FAIL|WARNING|===)'
            PASSED+=("$name")
        else
            echo "$out"
            FAILED+=("$name (exit $code)")
        fi
        echo
    else
        "$TIMEOUT_BIN" -k 2 --signal=INT "$TIMEOUT_SECS" "$VEMCODE_BIN" "$sketch" speed=5 timeout=2
        code=$?
        echo

        if [ "$code" -eq 124 ] || [ "$code" -eq 0 ] || [ "$code" -eq 137 ]; then
            PASSED+=("$name")
        else
            FAILED+=("$name (exit $code)")
        fi
    fi
done

echo "================ SUMMARY ================"
echo "Passed (${#PASSED[@]}): ${PASSED[*]:-none}"
echo "Failed (${#FAILED[@]}): ${FAILED[*]:-none}"

[ ${#FAILED[@]} -eq 0 ]
