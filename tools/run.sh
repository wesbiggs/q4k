#!/bin/sh
# Launch a .prg in VICE for interactive play.
set -eu
PRG="${1:?usage: tools/run.sh <file.prg> [extra vice args...]}"
shift

exec x64sc \
    -autostart "$PRG" \
    -autostartprgmode 1 \
    -VICIIdsize \
    "$@"
