#!/bin/sh
# Temporary diagnosis (not for main): openworld-out with parts of the
# branch put back as main has them.
M=454fe50
run() {
    echo "===== $1"
    make >/dev/null 2>&1 || { echo "build failed"; make 2>&1 | tail -20; }
    tools/test.sh openworld-out openworld-reu openworld-town 2>&1 | grep -v "^ *[0-9]* |" | head -20
    git checkout -q HEAD -- .
}
run head
git checkout $M -- src/ow/smooth.s tools/mkoverworld.py; run no-deep-water
git checkout $M -- src/ow/town.c; run no-town-c
git checkout $M -- src/ow/dungeon.c src/ow/engine.c; run no-engine-dungeon
git checkout $M -- src/lib/ow_disk.s src/openworld.c include/q4k/ow.h src/ow/engine.c src/ow/dungeon.c src/ow/town.c Makefile; run no-icon-at-all
git checkout $M -- src tools include Makefile assets cfg; run main
