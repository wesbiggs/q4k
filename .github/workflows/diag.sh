#!/bin/sh
# Temporary diagnosis (not for main): is openworld-out flaky under load?
for i in 1 2 3; do
    echo "===== goldens run $i"
    tools/test.sh --group goldens 2>&1 | grep "FAIL\|test.sh:"
done
echo "===== main, goldens"
git checkout -q 454fe50 -- src tools include Makefile assets cfg
make >/dev/null 2>&1 || echo "build failed"
for i in 1 2; do tools/test.sh --group goldens 2>&1 | grep "FAIL\|test.sh:"; done
