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
        # No output=$(...) capture -- command substitution always buffers the
        # whole child process's output until it exits, which is what caused
        # the "nothing, then everything at once" effect. Letting the child
        # inherit stdout/stderr directly instead prints live as it runs.
        "$TIMEOUT_BIN" -k 2 --signal=INT "$TIMEOUT_SECS" "$VEMCODE_BIN" "$sketch" timeline=true speed=10
        code=$?
        echo

        if [ "$code" -eq 0 ]; then
            PASSED+=("$name")
        else
            FAILED+=("$name (exit $code)")
        fi
    else
        "$TIMEOUT_BIN" -k 2 --signal=INT "$TIMEOUT_SECS" "$VEMCODE_BIN" "$sketch"
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
