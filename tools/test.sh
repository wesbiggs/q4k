#!/bin/sh
# Regression tests: run each case headlessly and compare the C64 screen
# against a golden text dump. Refresh goldens with tools/test.sh --bless,
# and only after reading the diff.
#
# A case is tests/golden/<name>.txt plus an optional <name>.cmd holding the
# screendump arguments. With no .cmd, the case just runs dist/<name>.prg.
set -eu
cd "$(dirname "$0")/.."

bless=0
[ "${1:-}" = "--bless" ] && bless=1
fail=0

# compare <name> <golden> <command...>
compare() {
    name=$1
    golden=$2
    shift 2

    # Capture the failure rather than letting `set -e` abort the whole run:
    # one case that will not boot should be one FAIL, not silence.
    if ! actual=$("$@" 2>/dev/null); then
        echo "FAIL  $name (did not complete)"
        fail=1
        return
    fi

    if [ "$bless" = 1 ]; then
        printf '%s\n' "$actual" > "$golden"
        echo "BLESS $name"
    elif [ "$actual" = "$(cat "$golden" 2>/dev/null)" ]; then
        echo "PASS  $name"
    else
        echo "FAIL  $name"
        printf '%s\n' "$actual" | diff -u "$golden" - || true
        fail=1
    fi
}

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
    if [ -f "tests/golden/$name.cmd" ]; then
        # shellcheck disable=SC2046
        set -- $(cat "tests/golden/$name.cmd")
    else
        set -- "dist/$name.prg"
    fi
    [ -f "$1" ] || { echo "SKIP  $name (no $1 -- run make)"; continue; }
    compare "$name" "$golden" tools/screendump.py "$@"
done

# Saving and restoring needs a disk in unit 8. A fresh one each run, so the
# case does not depend on what a previous run left lying about: walk two
# squares, save, walk two more, restore, and end up back where you saved.
#
# It walks north inside Millford rather than out onto the road. Overworld
# squares roll for wandering monsters, and now that the seed comes from the
# player's own timing the roll differs every run -- any test that walks in
# the open is flaky by construction. Towns never roll.
if [ -f tests/golden-save.txt ] && [ -f dist/q4k.prg ]; then
    saves=$(mktemp -t q4ksave).d64
    rm -f "$saves"
    c1541 -format "test,01" d64 "$saves" >/dev/null 2>&1
    compare save tests/golden-save.txt \
        tools/screendump.py dist/q4k.prg --disk "$saves" \
        --keys '\rww\x85ww\x86' --settle 8
    rm -f "$saves"
fi

# The dialogue case needs a daemon on the other end of the modem line, in
# --mock mode so the replies are the same every run.
if [ -f tests/golden-dialog.txt ] && [ -f dist/q4k.prg ]; then
    python3 host/npcd.py --mock --port 25299 --quiet >/dev/null 2>&1 &
    daemon=$!
    sleep 1
    compare dialog tests/golden-dialog.txt \
        tools/screendump.py dist/q4k.prg --rs232 25299 \
        --keys '\rwwwwwwwwwwwwaaaawaaawhat of the blade\r' --settle 32
    kill "$daemon" 2>/dev/null || true
    wait "$daemon" 2>/dev/null || true   # reap quietly; no job-control noise
fi

exit $fail
