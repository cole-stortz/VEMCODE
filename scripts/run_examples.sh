cd /home/cdstortz/VEMCODE
PASS=0
FAIL=0
for dir in app/sketches/examples/*/; do
  name=$(basename "$dir")
  cpp=$(find "$dir" -maxdepth 1 -name "*.cpp" | head -1)
  [ -z "$cpp" ] && continue
  out=$(timeout -k 1 4 ./app/VEMCODE "$cpp" timeout=2 speed=10 2>&1)
  code=$?
  if [ $code -ne 0 ] && [ $code -ne 124 ]; then
    echo "!!!! FAIL ($code): $name"
    echo "$out" | tail -6
    FAIL=$((FAIL+1))
  else
    echo "ok: $name"
    PASS=$((PASS+1))
  fi
done
echo "=== $PASS ok, $FAIL failed ==="