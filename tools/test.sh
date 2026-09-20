#!/bin/sh
# Regression tests: run each program headlessly and compare the C64 screen
# against a golden text dump. Refresh goldens with tools/test.sh --bless.
set -eu
cd "$(dirname "$0")/.."

bless=0
[ "${1:-}" = "--bless" ] && bless=1

fail=0

# Cheap and worth doing first: a hand-edited overworld can seal a town behind
# the mountains, and no screen test would ever notice.
if tools/checkmap.py >/dev/null 2>&1; then
    echo "PASS  checkmap"
else
    echo "FAIL  checkmap"
    tools/checkmap.py || true
    fail=1
fi

for golden in tests/golden/*.txt; do
    name=$(basename "$golden" .txt)
    prg="dist/$name.prg"
    [ -f "$prg" ] || { echo "SKIP $name (no $prg -- run make)"; continue; }

    # An optional <name>.args file carries extra screendump arguments, e.g.
    # a key sequence and the turn count to stop at.
    args=""
    [ -f "tests/golden/$name.args" ] && args=$(cat "tests/golden/$name.args")

    # Capture the failure rather than letting `set -e` abort the whole run:
    # one program that will not boot should be one FAIL, not silence.
    # shellcheck disable=SC2086
    if ! actual=$(tools/screendump.py "$prg" $args 2>/dev/null); then
        echo "FAIL  $name (screendump did not complete)"
        fail=1
        continue
    fi

    if [ "$bless" = 1 ]; then
        printf '%s\n' "$actual" > "$golden"
        echo "BLESS $name"
    elif [ "$actual" = "$(cat "$golden")" ]; then
        echo "PASS  $name"
    else
        echo "FAIL  $name"
        printf '%s\n' "$actual" | diff -u "$golden" - || true
        fail=1
    fi
done
# The dialogue test drives a full conversation over the emulated modem, with
# the daemon in --mock mode so replies are deterministic.
if [ -f tests/golden-dialog.txt ] && [ -f dist/q4k.prg ]; then
    python3 host/npcd.py --mock --port 25299 --quiet >/dev/null 2>&1 &
    daemon=$!
    sleep 1
    # Walk from the Millford arrival square to Garrett's forge, then talk.
    actual=$(tools/screendump.py dist/q4k.prg --rs232 25299 \
                 --keys 'wwwwwwwwwwwwaaaawaaawhat of the blade\r' \
                 --settle 32 2>/dev/null) || actual="(failed)"
    kill "$daemon" 2>/dev/null || true
    wait "$daemon" 2>/dev/null || true   # reap quietly; no job-control noise

    if [ "$bless" = 1 ]; then
        printf '%s\n' "$actual" > tests/golden-dialog.txt
        echo "BLESS dialog"
    elif [ "$actual" = "$(cat tests/golden-dialog.txt)" ]; then
        echo "PASS  dialog"
    else
        echo "FAIL  dialog"
        printf '%s\n' "$actual" | diff -u tests/golden-dialog.txt - || true
        fail=1
    fi
fi

exit $fail
