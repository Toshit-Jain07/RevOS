#!/bin/bash
# MiniOS regression tests.
# Runs every script and compares the output with the saved expected output.
#   demos/*.txt        -> results/*.out          (the demo transcripts)
#   tests/cases/*.txt  -> tests/expected/*.out   (bad input, lock/kill/deadlock interactions)
# Usage: make test      (or: bash tests/run_tests.sh)

cd "$(dirname "$0")/.."
mkdir -p build/test_out
pass=0
fail=0

for script in demos/*.txt; do
    name=$(basename "$script" .txt)
    ./minios < "$script" > "build/test_out/$name.out" 2>&1
    if diff -q "build/test_out/$name.out" "results/$name.out" > /dev/null; then
        echo "PASS  $script"
        pass=$((pass + 1))
    else
        echo "FAIL  $script   (see: diff build/test_out/$name.out results/$name.out)"
        fail=$((fail + 1))
    fi
done

for script in tests/cases/*.txt; do
    name=$(basename "$script" .txt)
    ./minios < "$script" > "build/test_out/$name.out" 2>&1
    if diff -q "build/test_out/$name.out" "tests/expected/$name.out" > /dev/null; then
        echo "PASS  $script"
        pass=$((pass + 1))
    else
        echo "FAIL  $script   (see: diff build/test_out/$name.out tests/expected/$name.out)"
        fail=$((fail + 1))
    fi
done

echo "----"
echo "$pass passed, $fail failed"
if [ "$fail" -ne 0 ]; then
    exit 1
fi
