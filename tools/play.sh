#!/bin/sh
# Start the dialogue daemon (if it isn't already up) and launch the game with
# its modem port wired to it.
#
#   tools/play.sh              live dialogue, needs ollama running
#   Q4K_MOCK=1 tools/play.sh   canned replies, no model needed
#
# Q4K_MODEL and Q4K_OLLAMA_URL are read by npcd.py itself, so they work here
# too; Q4K_PORT and Q4K_BAUD configure the emulated modem line.
set -eu
cd "$(dirname "$0")/.."

PORT="${Q4K_PORT:-25232}"
BAUD="${Q4K_BAUD:-300}"
daemon=""

if ! (exec 3<>/dev/tcp/127.0.0.1/"$PORT") 2>/dev/null; then
    # shellcheck disable=SC2086
    python3 host/npcd.py --port "$PORT" ${Q4K_MOCK:+--mock} &
    daemon=$!
    trap 'kill $daemon 2>/dev/null || true' EXIT INT TERM
    sleep 1
else
    echo "play.sh: reusing the daemon already on port $PORT"
fi

# The game loads its engines from its own disk, and saves to it too. Play
# from a copy, dist/saves.d64, so that rebuilding the game disk does not lose
# a save: when dist/q4k.d64 is newer, the copy is made afresh from it and the
# save (Q4K.SAV) carried over.
GAME="dist/q4k.d64"
SAVES="dist/saves.d64"
if [ ! -f "$SAVES" ] || [ "$GAME" -nt "$SAVES" ]; then
    kept=$(mktemp "${TMPDIR:-/tmp}/q4ksav.XXXXXX")
    have=
    if [ -f "$SAVES" ] \
       && c1541 -attach "$SAVES" -read q4k.sav "$kept" >/dev/null 2>&1 \
       && [ -s "$kept" ]; then
        have=1
    fi
    cp "$GAME" "$SAVES"
    if [ -n "$have" ]; then
        c1541 -attach "$SAVES" -write "$kept" q4k.sav >/dev/null
        echo "play.sh: $SAVES has the new game, and your save"
    else
        echo "play.sh: created $SAVES to play from; saves go to it"
    fi
    rm -f "$kept"
fi

tools/run.sh "$SAVES" \
    -userportdevice 2 \
    -rsuserdev 0 \
    -rsuserbaud "$BAUD" \
    -rsdev1 "127.0.0.1:$PORT"
