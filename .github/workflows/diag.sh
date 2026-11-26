#!/bin/sh
# Temporary diagnosis (not for main): runs one quick start with OW-CPU in
# several variants and prints what it finds.
run() {
    echo "===== $1"
    make >/dev/null 2>&1 || { echo "build failed"; make 2>&1 | tail -20; return; }
    grep -A40 "^Segment list" build/ow-cpu.map | head -45
    timeout 120 tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --settle 6 --timeout 60 \
        --peek '$0314:2' --peek '$0316:2' --peek '$07F8' --peek '$D015' --peek '$D011' \
        --peek '$CF00:2' --peek '$CF02' --peek '$CF03' --peek '$CF1A' --peek '$0002:2' 2>&1 | tail -45
}
run head
git checkout 454fe50 -- src/ow/smooth.s; run no-smooth; git checkout HEAD -- src/ow/smooth.s
sed -i 's/^    need("ow-icon", 0);/    \/\* no icon \*\//' src/openworld.c; run no-icon-load; git checkout HEAD -- src/openworld.c
git checkout 454fe50 -- src/ow/engine.c; run old-engine-c; git checkout HEAD -- src/ow/engine.c
git checkout 454fe50 -- src tools include Makefile; run main; git checkout HEAD -- src tools include Makefile
