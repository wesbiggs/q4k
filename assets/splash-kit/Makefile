# splash-demo.prg: the example host program with the splash (needs cc65's cl65)
#   make                       PAL and NTSC images, 5 seconds or space
#   make SECS=0                until space
#   make PAL=0 (or NTSC=0)     one image only (20K less)
SECS ?= 5
PAL  ?= 1
NTSC ?= 1
KEY  ?= 1

splash-demo.prg: splash-demo.s lib/splash.inc lib/v2i.inc lib/v2t.inc lib/font.inc \
                 $(wildcard data/*) c64.cfg Makefile
	cl65 -t none -C c64.cfg --asm-include-dir lib --bin-include-dir data \
	    --asm-define SPLASH_SECS=$(SECS) --asm-define SPLASH_PAL=$(PAL) \
	    --asm-define SPLASH_NTSC=$(NTSC) --asm-define SPLASH_KEY=$(KEY) \
	    -o $@ splash-demo.s
	rm -f *.o

run: splash-demo.prg
	x64sc -autostartprgmode 1 splash-demo.prg

clean:
	rm -f splash-demo.prg *.o

.PHONY: splash-demo.prg run clean     # always rebuilt: the options aren't files
