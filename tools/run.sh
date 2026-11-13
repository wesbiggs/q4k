#!/bin/sh
# Launch a .prg, or a .d64 (the game, which loads its engines from its disk),
# in VICE for interactive play.
set -eu
PRG="${1:?usage: tools/run.sh <file.prg|file.d64> [extra vice args...]}"
shift

exec x64sc \
    -autostart "$PRG" \
    -autostartprgmode 1 \
    -VICIIdsize \
    "$@"
