#!/bin/sh
# Headless: boot the .prg, run for a fixed number of emulated frames at warp
# speed, save a PNG of the screen, exit. Deterministic -- same input, same PNG.
set -eu
PRG="${1:?usage: tools/screenshot.sh <file.prg> [out.png]}"
OUT="${2:-screen.png}"
FRAMES="${FRAMES:-240}"

# PAL C64: 63 cycles/line * 312 lines = 19656 cycles per frame.
CYCLES=$(( FRAMES * 19656 ))

mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"

x64sc \
    -default \
    -autostart "$PRG" \
    -autostartprgmode 1 \
    -warp \
    +sound \
    -limitcycles "$CYCLES" \
    -exitscreenshot "$OUT" \
    >/dev/null 2>&1 || true

[ -s "$OUT" ] || { echo "screenshot.sh: no output at $OUT" >&2; exit 1; }
