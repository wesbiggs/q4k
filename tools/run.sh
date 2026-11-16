#!/bin/sh
# Launch a .prg, or a .d64 (the game, which loads its engines from its disk),
# in VICE for interactive play.
set -eu
PRG="${1:?usage: tools/run.sh <file.prg|file.d64> [extra vice args...]}"
shift

# A D81 (the open world's) wants a 1581 in unit 8.
case "$PRG" in
    *.d81|*.D81) set -- -drive8type 1581 "$@" ;;
esac

exec x64sc \
    -autostart "$PRG" \
    -autostartprgmode 1 \
    -VICIIdsize \
    "$@"
