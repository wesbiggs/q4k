# q4k -- Commodore 64 build
#
# Layout:
#   src/<name>.c    a program with main(); builds to dist/<name>.prg
#   src/lib/*.c|*.s shared modules; archived into a .lib so the linker
#                   pulls in only what each program actually references
#
# Usage:
#   make            build every program
#   make run        launch PROG in VICE  (make run PROG=hello)
#   make play       launch the game with the NPC dialogue daemon attached
#   make check      headless run, dump the C64 screen as text
#   make test       headless run, diff every program against its golden
#   make bless      accept current output as the new goldens
#   make assets     regenerate the C and JSON built from assets/
#   make shot       headless run, save a PNG screenshot
#   make d64        build a bootable disk image
#   make release    the files a release publishes, in dist/release/
#   make clean

TARGET   := c64
PROG     ?= hello

BUILD    := build
DIST     := dist

CL65     := cl65
AR65     := ar65
C1541    := c1541

# -Osir: optimize, inline known routines, use register vars where it pays.
# --static-locals: locals become static storage -- faster on 6502 (no stack
# frame juggling) at the cost of no recursion. Right trade for a tile RPG.
CFLAGS   := -t $(TARGET) -Osir --static-locals -I include
ASFLAGS  := -t $(TARGET) -I include --asm-include-dir include
# The game gets the full memory map; the demos and tools get a lean one that
# skips the HIGH region and its contiguity padding.
LDFLAGS  := -t $(TARGET)
# Have the compiler write each object's header dependencies next to it, so
# that editing a header -- or regenerating one from assets/, like tiles.h --
# rebuilds every object that includes it. "Full" because the project's own
# headers are included as <q4k/...>, which plain --create-dep skips as
# system headers. Each header also gets an empty rule, so deleting one does
# not break the build.
DEPFLAGS  = -Wc --create-full-dep,$(@:.o=.d) -Wc --dep-target,$@
# dungeontest links the maps and the font, and arena the fights, so both need
# the game's map too.
LDCFG     = cfg/c64-$(if $(filter q4k dungeontest arena isletest,$*),q4k,tool).cfg

MAINS    := $(wildcard src/*.c)
PRGS     := $(patsubst src/%.c,$(DIST)/%.prg,$(MAINS))

# Modules whose code and constants live above the character set, in the HIGH
# region. Twelve kilobytes under the charset does not hold the whole game;
# these are the biggest and least performance-critical parts (menus and
# set-piece screens), so putting them out of the way costs nothing. Both
# flags are needed: --code-name alone leaves the string literals behind, and
# screens like the help text are mostly string literals.
HIMODS   := dialog battle shop inn dungeon panel title help savegame sprite \
            loot fov world sail

# Code above the charset, constants below: for when HIGH is the one that is
# full and MAIN still has room. The shop's names and prices went back down
# when the dungeon work and the version check together overflowed HIGH by
# 88 bytes; the ship's messages did the same for sail.
HICODEMODS := shop sail

LIBSRC   := $(wildcard src/lib/*.c) $(wildcard src/lib/*.s)
LIBOBJ   := $(patsubst src/lib/%.c,$(BUILD)/lib/%.o,$(wildcard src/lib/*.c)) \
            $(patsubst src/lib/%.s,$(BUILD)/lib/%.o,$(wildcard src/lib/*.s))

$(foreach m,$(filter-out $(HICODEMODS),$(HIMODS)),$(eval $(BUILD)/lib/$(m).o: CFLAGS += --code-name HICODE --rodata-name HIDATA))
$(foreach m,$(HICODEMODS),$(eval $(BUILD)/lib/$(m).o: CFLAGS += --code-name HICODE))

ifneq ($(strip $(LIBOBJ)),)
LIB      := $(BUILD)/q4k.lib
endif

.PHONY: all run play check shot test bless d64 release clean dirs assets
.SECONDARY: $(BUILD)/$(PROG).o

all: dirs assets $(PRGS)

# Assembly cannot see tiles.h; it has its own generated copy of the codes.
$(BUILD)/lib/isle_gen.o: include/q4k/tiles.inc

# --- generated from assets ---------------------------------------------------
#
# The generated C and the geography the NPCs are given are checked in, so a
# plain build needs no Python. These rules keep them from going stale: edit
# an asset and the next make regenerates whatever depended on it.

GEN_TILES := include/q4k/tiles.h include/q4k/tiles.inc src/lib/tiles_data.c \
            src/lib/tiles_names.c
GEN_MAP   := include/q4k/world_data.h src/lib/map_data.c
GEN_TOWN  := include/q4k/town_data.h src/lib/town_data.c
GEN_ARENA := include/q4k/arena_data.h src/lib/arena_data.c
GEN_GEO   := assets/geography.json
GEN_FONT  := src/lib/font_data.c
GEN_LOOT  := include/q4k/loot_data.h src/lib/loot_data.c
GEN_VER   := include/q4k/version.h
GEN_MUSIC := src/lib/music_data.s

# Where the letters come from. FONT_SOURCE is "rom" for the machine's own
# character set, or a path to a C64 charset binary. FONT_STYLE only applies
# to the rom source: "serif" puts a foot on the stems, "bold" thickens the
# strokes first and then does the same. W and w are always taken from the
# ROM unchanged -- thickening four diagonals in six pixels fills the gaps
# between them.
#
# The default is the ROM because a third-party charset is somebody's
# typeface and usually not ours to ship. See assets/fonts/NOTICE.md.
FONT_SOURCE ?= rom
FONT_STYLE  ?= bold

assets: $(GEN_TILES) $(GEN_MAP) $(GEN_TOWN) $(GEN_ARENA) $(GEN_GEO) $(GEN_FONT) \
        $(GEN_LOOT) $(GEN_VER) $(GEN_MUSIC)

# The version lives in VERSION alone; the release workflow checks the tag
# against it. No Python needed, so this one is plain shell.
$(GEN_VER): VERSION
	@printf '%s\n' '/* GENERATED from VERSION by the Makefile -- do not edit. */' \
	    '#ifndef Q4K_VERSION_H' '#define Q4K_VERSION_H' '' \
	    "#define Q4K_VERSION \"$$(tr -d ' \n' < VERSION)\"" '' \
	    '#endif' > $@
	@echo "version: $$(cat VERSION)"

$(GEN_TILES): assets/tiles.txt tools/mktiles.py
	tools/mktiles.py

$(GEN_MAP): assets/overworld.txt assets/tiles.txt tools/mkmap.py tools/mktiles.py
	tools/mkmap.py

$(GEN_TOWN): assets/towns.txt assets/tiles.txt assets/npcs.json tools/mktowns.py \
             tools/rle.py
	tools/mktowns.py

$(GEN_ARENA): assets/arenas.txt assets/tiles.txt tools/mkarenas.py \
              tools/mktiles.py tools/checkmap.py tools/rle.py
	tools/mkarenas.py

$(GEN_LOOT): assets/loot.txt assets/towns.txt assets/overworld.txt \
             assets/tiles.txt assets/npcs.json tools/mkloot.py tools/mktowns.py \
             tools/mktiles.py
	tools/mkloot.py

$(GEN_MUSIC): assets/theme.txt tools/mkmusic.py
	tools/mkmusic.py

$(GEN_GEO): assets/overworld.txt assets/towns.txt assets/places.txt \
            assets/tiles.txt tools/mkgeo.py
	tools/mkgeo.py

# A stamp holding the chosen font, so that changing FONT_SOURCE on the
# command line actually rebuilds. Make compares file times, not variables:
# without this, `make assets FONT_SOURCE=rom` would decide the font was
# already up to date and quietly do nothing.
FONT_CONF := $(FONT_SOURCE) $(FONT_STYLE)

.PHONY: FORCE
FORCE:

$(BUILD)/font.conf: FORCE
	@mkdir -p $(BUILD)
	@printf '%s\n' '$(FONT_CONF)' | cmp -s - $@ || printf '%s\n' '$(FONT_CONF)' > $@

$(GEN_FONT): tools/mkfont.py $(BUILD)/font.conf $(wildcard assets/fonts/*.bin)
	tools/mkfont.py --source $(FONT_SOURCE) --style $(FONT_STYLE)

dirs:
	@mkdir -p $(BUILD)/lib $(DIST)

# --- shared library ---------------------------------------------------------

$(BUILD)/lib/%.o: src/lib/%.c
	@mkdir -p $(dir $@)
	$(CL65) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD)/lib/%.o: src/lib/%.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) -c -o $@ $<

ifdef LIB
$(LIB): $(LIBOBJ)
	@rm -f $@
	$(AR65) a $@ $(LIBOBJ)
endif

# --- programs ---------------------------------------------------------------

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CL65) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

-include $(patsubst src/%.c,$(BUILD)/%.d,$(MAINS)) \
         $(patsubst src/lib/%.c,$(BUILD)/lib/%.d,$(wildcard src/lib/*.c))

$(DIST)/%.prg: $(BUILD)/%.o $(LIB) $(wildcard cfg/*.cfg)
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C $(LDCFG) -m $(BUILD)/$*.map \
	    -Wl -Ln,$(BUILD)/$*.lbl -o $@ $< $(LIB)
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"

# --- run / verify -----------------------------------------------------------

run: dirs $(DIST)/$(PROG).prg
	tools/run.sh $(DIST)/$(PROG).prg

# Play with NPC dialogue: starts host/npcd.py and wires it to the modem port.
# Q4K_MOCK=1 make play  uses canned replies instead of calling the model.
play: dirs $(DIST)/q4k.prg
	tools/play.sh

check: dirs $(DIST)/$(PROG).prg
	tools/screendump.py $(DIST)/$(PROG).prg

shot: dirs $(DIST)/$(PROG).prg
	tools/screenshot.sh $(DIST)/$(PROG).prg $(BUILD)/$(PROG).png
	@echo "wrote $(BUILD)/$(PROG).png"

# Compare each program's screen against tests/golden/<name>.txt.
test: all
	@tools/test.sh

# Accept the current output as correct. Review the diff before doing this.
bless: all
	@tools/test.sh --bless

# --- release ----------------------------------------------------------------
#
# What .github/workflows/release.yml publishes for a v* tag, and what a
# player downloads instead of building:
#
#   q4k-<version>.prg   the game, for VICE or any loader
#   q4k-<version>.d64   the same on a disk: LOAD"Q4K",8,1 then RUN
#   q4k-<version>.zip   the game plus the dialogue host and what it reads,
#                       laid out as in the repo, so tools/play.sh runs as is

VERSION_NUM := $(shell tr -d ' \n' < VERSION)
REL_NAME    := q4k-$(VERSION_NUM)
REL_DIR     := $(DIST)/release
REL_STAGE   := $(BUILD)/release/$(REL_NAME)
REL_FILES   := README.md host/npcd.py tools/play.sh tools/run.sh \
               assets/canon.md assets/npcs.json assets/secrets.json \
               assets/hints.json assets/geography.json

release: dirs $(DIST)/q4k.prg
	@rm -rf $(BUILD)/release $(REL_DIR)
	@mkdir -p $(REL_DIR) $(REL_STAGE)/dist
	cp $(DIST)/q4k.prg $(REL_DIR)/$(REL_NAME).prg
	$(C1541) -format "q4k $(VERSION_NUM),01" d64 $(REL_DIR)/$(REL_NAME).d64 >/dev/null
	$(C1541) -attach $(REL_DIR)/$(REL_NAME).d64 -write $(DIST)/q4k.prg q4k >/dev/null
	tar -cf - $(REL_FILES) | tar -xf - -C $(REL_STAGE)
	cp $(DIST)/q4k.prg $(REL_STAGE)/dist/q4k.prg
	cd $(BUILD)/release && python3 -m zipfile -c $(CURDIR)/$(REL_DIR)/$(REL_NAME).zip $(REL_NAME)
	@ls -l $(REL_DIR)

# --- disk image -------------------------------------------------------------

d64: dirs $(DIST)/$(PROG).prg
	@rm -f $(DIST)/$(PROG).d64
	$(C1541) -format "q4k,01" d64 $(DIST)/$(PROG).d64 >/dev/null
	$(C1541) -attach $(DIST)/$(PROG).d64 -write $(DIST)/$(PROG).prg $(PROG) >/dev/null
	@echo "wrote $(DIST)/$(PROG).d64   (LOAD\"$(PROG)\",8,1 then RUN)"

clean:
	rm -rf $(BUILD) $(DIST)
