# SPDX-License-Identifier: GPL-3.0-or-later
# q4k -- Commodore 64 build: the open world
#
# Layout:
#   src/<name>.c    a program with main(); builds to dist/<name>.prg
#   src/lib/*.c|*.s shared modules; archived into a .lib so the linker
#                   pulls in only what each program actually references
#   src/ow/         the open world's engine; src/fight/ its fight module
#
# Usage:
#   make            build everything: dist/openworld.d81, dist/owarena.d64
#   make run        launch PROG in VICE (default openworld; PROG=owarena)
#   make playtown   play-test one town: TOWN=castle (AT=x,y), then F3
#   make test       headless run, diff every case against its golden
#   make bless      accept current output as the new goldens
#   make assets     regenerate the C and assembly built from assets/
#   make clean

TARGET   := c64
PROG     ?= openworld

BUILD    := build
DIST     := dist

# What people say is on the disk, a file each, which src/lib/talk.c loads
# into the talk's buffer (src/lib/talkbuf.s; tools/mktalk.py makes the files
# from assets/talk.txt, and the inns' rumours from assets/hints.json), one
# person's lines or one inn's rumours at a time. mktalk.py checks every file
# against it.
TALK_SIZE := 0x0BEC

CL65     := cl65
AR65     := ar65
C1541    := c1541

# -Osir: optimize, inline known routines, use register vars where it pays.
# --static-locals: locals become static storage -- faster on 6502 (no stack
# frame juggling) at the cost of no recursion. Right trade for a tile RPG.
OPT      := -Osir
CFLAGS    = -t $(TARGET) $(OPT) --static-locals -I include
ASFLAGS  := -t $(TARGET) -I include --asm-include-dir include \
            --asm-define TALK_SIZE=$(TALK_SIZE)
# Every program has a memory map of its own (cfg/): the bootstrap's, the
# engines', owtown's, owtalk's, the arena's; isletest has everything in one
# load (cfg/c64-flat.cfg) and owparty and the small ones the lean tool map.
LDFLAGS  := -t $(TARGET)
# Have the compiler write each object's header dependencies next to it, so
# that editing a header -- or regenerating one from assets/, like tiles.h --
# rebuilds every object that includes it. "Full" because the project's own
# headers are included as <q4k/...>, which plain --create-dep skips as
# system headers. Each header also gets an empty rule, so deleting one does
# not break the build.
DEPFLAGS  = -Wc --create-full-dep,$(@:.o=.d) -Wc --dep-target,$@
LDCFG     = cfg/c64-$(if $(filter isletest,$*),flat,$(if $(filter owtown owtalk,$*),$*,$(if $(filter openworld,$*),owboot,$(if $(filter owarena,$*),owarena,tool)))).cfg

MAINS    := $(wildcard src/*.c)
PRGS     := $(patsubst src/%.c,$(DIST)/%.prg,$(MAINS))

# Modules whose code and constants live in the HIGH segments, which each
# program's cfg places (in owtown and owtalk, above the character set). Both
# flags are needed: --code-name alone leaves the string literals behind. (A
# module in an ENG_ list, below, goes to that list's segments instead.)
HIMODS   := arms panel actor quest ow_figure

# These are compiled without -Os, which unrolls shifts and multiplies by
# constants and costs about a tenth of their size: the screens of text lose
# nothing by it. Not the generators, whose timing was measured with it.
# Changing this list needs the objects rebuilt: flags are not dependencies.
SMALLMODS  := shop inn help arms talk gift ow_tmap
$(foreach m,$(SMALLMODS),$(eval $(BUILD)/lib/$(m).o: OPT := -Or))

# What copies the character set in (charset_define.o's charset_install())
# is wanted once, at the start of a program, in a segment of its own.
$(BUILD)/lib/charset_define.o: CFLAGS += --code-name TITLECODE

LIBSRC   := $(wildcard src/lib/*.c) $(wildcard src/lib/*.s)
LIBOBJ   := $(patsubst src/lib/%.c,$(BUILD)/lib/%.o,$(wildcard src/lib/*.c)) \
            $(patsubst src/lib/%.s,$(BUILD)/lib/%.o,$(wildcard src/lib/*.s))

# dungeon.c, sfx.s and the towns' maps are built into the engine instead
# (ow_dungeon.o, ow_sfx.o, ow_town_maps.o, below), and nothing links the
# library's copies.
LIBOBJ   := $(filter-out $(BUILD)/lib/dungeon.o $(BUILD)/lib/sfx.o \
              $(BUILD)/lib/town_maps.o,$(LIBOBJ))

# The segments the modules of the screens of text (owtown, owtalk) and the
# towns' tables are compiled into: names the game's engines once had, which
# the open world's cfgs place. Their variables stay in BSS and DATA.
ENG_MAP   := town_data town_gen loot_data
ENG_FIGHT := bestiary
ENG_TALK  := talk talkscr inn gift
ENG_SHOP  := shop help ow_tmap
ENGMODS   := $(ENG_MAP) $(ENG_FIGHT) $(ENG_TALK) $(ENG_SHOP)

$(foreach e,MAP FIGHT TALK SHOP,$(foreach m,$(ENG_$(e)),$(eval $(BUILD)/lib/$(m).o: CFLAGS += --code-name $(e)CODE --rodata-name $(e)DATA)))
$(foreach m,$(filter-out $(ENGMODS),$(HIMODS)),$(eval $(BUILD)/lib/$(m).o: CFLAGS += --code-name HICODE --rodata-name HIDATA))

ifneq ($(strip $(LIBOBJ)),)
LIB      := $(BUILD)/q4k.lib
endif

.PHONY: all run playtown test bless clean dirs assets
.SECONDARY: $(BUILD)/$(PROG).o

# The open world's memory map, a page drawn from the links below.
MEMMAP := docs/memmap.html

all: dirs assets $(PRGS) $(DIST)/openworld.d81 $(BUILD)/owcache.reu $(DIST)/owarena.d64 \
     $(DIST)/theme.sid $(BUILD)/music/.stamp $(MEMMAP)

# Assembly cannot see tiles.h; it has its own generated copy of the codes.
$(BUILD)/lib/isle_gen.o: include/q4k/tiles.inc

# --- generated from assets ---------------------------------------------------
#
# The generated C and assembly are checked in. These rules keep them from going stale: edit
# an asset and the next make regenerates whatever depended on it.

GEN_TILES := include/q4k/tiles.h include/q4k/tiles.inc src/lib/tiles_data.c
GEN_TOWN  := include/q4k/town_data.h src/lib/town_data.c src/lib/town_maps.c
GEN_NAMES := include/q4k/names_data.h
GEN_FONT  := src/lib/font_data.c
GEN_CVT   := assets/q4k-serif.cvt
GEN_LOOT  := include/q4k/loot_data.h src/lib/loot_data.c
GEN_MUSIC := src/sid/theme_data.s
GEN_TUNES := include/q4k/music_map.h src/lib/music_map.c
GEN_OW    := src/lib/ow_data.s src/lib/ow_figs.s src/ow/town_art.s
GEN_FIGHT := src/fight/fight_data.s src/ow/foe_art.s
GEN_OWFONT := src/ow/ow_font.s

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

assets: $(GEN_TILES) $(GEN_TOWN) $(GEN_NAMES) $(GEN_FONT) $(GEN_CVT) \
        $(GEN_LOOT) $(GEN_MUSIC) $(GEN_TUNES) $(GEN_OW) $(GEN_FIGHT) \
        $(GEN_OWFONT)

$(GEN_TILES): assets/tiles.txt tools/mktiles.py
	tools/mktiles.py

$(GEN_TOWN): assets/towns.txt assets/tiles.txt assets/npcs.json tools/mktowns.py \
             tools/rle.py
	tools/mktowns.py

$(GEN_LOOT): assets/loot.txt assets/towns.txt \
             assets/tiles.txt assets/npcs.json tools/mkloot.py tools/mktowns.py \
             tools/mktiles.py
	tools/mkloot.py

$(GEN_MUSIC): assets/theme.txt tools/mkmusic.py
	tools/mkmusic.py

$(GEN_TUNES): assets/music.txt tools/mksid.py
	tools/mksid.py map

$(GEN_OW): assets/overworld.txt $(wildcard assets/*_tiles.txt) \
           assets/common_tiles.txt assets/tiles.txt assets/monsters.txt \
           tools/mkoverworld.py tools/figures.py
	tools/mkoverworld.py

# The engine's text window's letters: the game's own, and the party's
# lines' draught and arrow.
$(GEN_OWFONT): $(GEN_FONT) tools/mkowfont.py
	tools/mkowfont.py

# The open world's tile sets, a file each on its disk (t.overworld ...),
# which the engine loads into SHORE on the way into an area.
OWSETS := $(BUILD)/ow/sets/.stamp

$(OWSETS): assets/overworld.txt $(wildcard assets/*_tiles.txt) \
           assets/common_tiles.txt assets/tiles.txt assets/monsters.txt \
           tools/mkoverworld.py tools/figures.py
	@mkdir -p $(@D)
	tools/mkoverworld.py --sets $(@D)
	@touch $@

# The multicolour arena's sprites (the ones linked into the fight), and
# further down the files of its disk: the fields, drawn with the prototype's
# tile sets, and each monster's sprites.
FIELD_SRC := assets/fields.txt assets/monsters.txt assets/field_tiles.txt \
             assets/overworld.txt assets/dungeon_tiles.txt \
             assets/common_tiles.txt assets/tiles.txt tools/mkfields.py \
             tools/mkoverworld.py tools/figures.py

$(GEN_FIGHT): $(FIELD_SRC)
	tools/mkfields.py

$(GEN_NAMES): assets/names.txt tools/mknames.py
	tools/mknames.py

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

# The same letters as a proportional GEOS font in Convert format, for
# vic2orious's v2t text (`build --font assets/q4k-serif.cvt`).
$(GEN_CVT): $(GEN_FONT) tools/mkcvt.py
	tools/mkcvt.py

dirs:
	@mkdir -p $(BUILD)/lib $(DIST)

# --- shared library ---------------------------------------------------------

$(BUILD)/lib/%.o: src/lib/%.c
	@mkdir -p $(dir $@)
	$(CL65) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD)/lib/%.o: src/lib/%.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) -c -o $@ $<

# The prototype's splash screen is the splash kit's libraries (assets/
# splash-kit/lib, made with vic2orious) with a routine of ours round them.
# -t none, not c64: they compare the files' magic with ASCII strings.
SPLASHKIT := assets/splash-kit
$(BUILD)/lib/ow_splash.o: ASFLAGS := -t none --asm-include-dir $(SPLASHKIT)/lib
$(BUILD)/lib/ow_splash.o: $(wildcard $(SPLASHKIT)/lib/*.inc)

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

# The smooth-scrolling prototype: a bootstrap (openworld.prg, built as any
# program here) and two engines from one source, with and without the REU
# (src/ow/, -DOW_REU and --asm-define OW_REU), each with a label file for
# the tests. The bootstrap loads one (ow.h). What is the same in both -- the
# tile sets and the towns' maps -- ld65 writes to a file beside each
# (cfg/c64-openworld.cfg's COMMON), and the disk has it once, as OW-DATA,
# which the bootstrap loads: the two must be the same bytes. The disk also
# has the splash screen's two pictures (PAL and NTSC), each with its text,
# and a saved game to load until the player saves one. It is a D81 (a 1581,
# 3160 blocks): with the fights a 1541's disk was full.
OWENG := $(DIST)/ow-cpu.prg $(DIST)/ow-reu.prg
OWOVL := $(foreach e,cpu reu,$(foreach o,world town dungeon hi tl,$(DIST)/ow-$(e).$(o)))
OWFILES := $(DIST)/ow-data.prg $(DIST)/splash-p.prg $(DIST)/splash-n.prg \
           $(DIST)/ow-save.prg $(DIST)/ow-isl.prg

# The open sea's islands' drawing (q4k/isles.h): a file of its own, which
# the bootstrap loads under the KERNAL, where it runs.
$(DIST)/ow-isl.prg: src/isl/isl_draw.s cfg/isl.cfg
	@mkdir -p $(DIST) $(BUILD)/isl
	$(CL65) -t none -C cfg/isl.cfg -m $(BUILD)/isl/isl.map -o $@ $<

# Each engine is a core (engine.c, smooth.s) and three overlays that take
# turns in one slot (src/ow/engine.h): the world's (world.c, OVWCODE), the
# town's (town.c, OVTCODE) and the dungeon's (dungeon.c, OVDCODE), which
# ld65 writes beside it as %O.world, %O.town and %O.dungeon, and
# tools/owslots.py checks that nothing calls into an overlay but through its
# jump table (ovl.s).
OWPARTS := engine world town dungeon

$(BUILD)/ow/world-cpu.o $(BUILD)/ow/world-reu.o: OWSEG := --code-name OVWCODE
$(BUILD)/ow/town-cpu.o $(BUILD)/ow/town-reu.o: OWSEG := --code-name OVTCODE
$(BUILD)/ow/dungeon-cpu.o $(BUILD)/ow/dungeon-reu.o: OWSEG := --code-name OVDCODE

# The dungeon generator, src/lib/dungeon.c, in the dungeon's overlay. Its
# constants stay in COMMON (MAPDATA), the same bytes in both engines.
$(BUILD)/ow/ow_dungeon.o: src/lib/dungeon.c
	@mkdir -p $(dir $@)
	$(CL65) $(filter-out -Osir,$(CFLAGS)) -Or --code-name OVDCODE \
	    --rodata-name MAPDATA $(DEPFLAGS) -c -o $@ $<

# The generators the open world builds with, in the overlay that uses them:
# the land's (isle_gen.s, OW_WORLD) and the build's thread (genbuild.s) in
# the world's, the towns' (tgen.s, OW_TOWN, town_gen.c, town_data.c and
# the maps, town_maps.c) in the town's. Linked ahead of the library, so
# that its MAPCODE copies are not; tools/owslots.py knows them by name.
# The C ones through build/ow/NAME.s, never src/lib/NAME.s (cl65 writes
# that for the library's).
$(BUILD)/ow/ow_isle.o: src/lib/isle_gen.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) --asm-define OW_WORLD=1 -c -o $@ $<
$(BUILD)/ow/ow_tgen.o: src/lib/tgen.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) --asm-define OW_TOWN=1 -c -o $@ $<
$(BUILD)/ow/ow_town_gen.o $(BUILD)/ow/ow_town_data.o $(BUILD)/ow/ow_town_maps.o: $(BUILD)/ow/ow_%.o: src/lib/%.c
	@mkdir -p $(dir $@)
	cc65 $(CFLAGS) --code-name OVTCODE --rodata-name OVTCODE \
	    --create-full-dep $(@:.o=.d) --dep-target $@ -o $(@:.o=.s) $<
	ca65 -t c64 -o $@ $(@:.o=.s)

# Each part is compiled twice, and cl65 would write both compilations'
# assembly to the same src/ow/NAME.s: under make -j the two raced, and an
# object could come out of the other engine's code, or not assemble. So
# cc65 and ca65 separately, each through a .s of its own in build/.
define OWPART
$(BUILD)/ow/$(1)-cpu.o $(BUILD)/ow/$(1)-reu.o: $(BUILD)/ow/$(1)-%.o: src/ow/$(1).c
	@mkdir -p $$(dir $$@)
	cc65 $$(filter-out -Osir,$$(CFLAGS)) -Or $$(if $$(filter reu,$$*),-DOW_REU) \
	    $$(OWSEG) --create-full-dep $$(@:.o=.d) --dep-target $$@ -o $$(@:.o=.s) $$<
	ca65 -t c64 -o $$@ $$(@:.o=.s)
endef
$(foreach p,$(OWPARTS),$(eval $(call OWPART,$(p))))

$(BUILD)/ow/smooth-cpu.o $(BUILD)/ow/smooth-reu.o: $(BUILD)/ow/smooth-%.o: src/ow/smooth.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) --asm-define OW_REU=$(if $(filter reu,$*),1,0) -c -o $@ $<

$(BUILD)/ow/ovl.o: src/ow/ovl.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) -c -o $@ $<

# The dungeon's monsters' sprites (tools/mkfields.py), and what they do
# square by square (foes.s), in its overlay; the king's (mkoverworld.py),
# in the town's.
$(BUILD)/ow/foe_art.o $(BUILD)/ow/town_art.o $(BUILD)/ow/foes.o $(BUILD)/ow/stlow.o $(BUILD)/ow/look.o $(BUILD)/ow/genbuild.o $(BUILD)/ow/wfog.o $(BUILD)/ow/ow_font.o: $(BUILD)/ow/%.o: src/ow/%.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) -c -o $@ $<

# The sound effects (src/lib/sfx.s), in the engine's own segments and
# ticking with its music.
$(BUILD)/ow/ow_sfx.o: src/lib/sfx.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) --asm-define OW_SFX=1 -c -o $@ $<

OWGEN := $(patsubst %,$(BUILD)/ow/%.o,ow_isle genbuild wfog ow_tgen ow_town_gen ow_town_data ow_town_maps)
OWOBJ = $(foreach p,$(OWPARTS) smooth,$(BUILD)/ow/$(p)-$*.o) $(BUILD)/ow/ovl.o \
        $(BUILD)/ow/ow_dungeon.o $(BUILD)/ow/ow_sfx.o $(BUILD)/ow/foe_art.o \
        $(BUILD)/ow/town_art.o $(BUILD)/ow/foes.o $(BUILD)/ow/stlow.o $(BUILD)/ow/look.o \
        $(BUILD)/ow/ow_font.o $(OWGEN)

# The overlays come out of the same link (ld65's %O.world, ...).
$(DIST)/ow-%.world $(DIST)/ow-%.town $(DIST)/ow-%.dungeon $(DIST)/ow-%.hi $(DIST)/ow-%.tl: $(DIST)/ow-%.prg ;

$(DIST)/ow-%.prg: $(foreach p,$(OWPARTS) smooth,$(BUILD)/ow/$(p)-%.o) $(BUILD)/ow/ovl.o \
	    $(BUILD)/ow/ow_dungeon.o $(BUILD)/ow/ow_sfx.o $(BUILD)/ow/foe_art.o $(BUILD)/ow/foes.o \
	    $(BUILD)/ow/town_art.o $(BUILD)/ow/stlow.o $(BUILD)/ow/look.o $(BUILD)/ow/ow_font.o $(OWGEN) $(LIB) cfg/c64-openworld.cfg tools/owslots.py
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C cfg/c64-openworld.cfg -m $(BUILD)/ow-$*.map \
	    -Wl -Ln,$(BUILD)/ow-$*.lbl -o $@ $(OWOBJ) $(LIB)
	@mv $@.data $(BUILD)/ow-$*.data
	@mv $@.world $(DIST)/ow-$*.world
	@mv $@.town $(DIST)/ow-$*.town
	@mv $@.dungeon $(DIST)/ow-$*.dungeon
	@mv $@.hi $(DIST)/ow-$*.hi
	@mv $@.tl $(DIST)/ow-$*.tl
	tools/owslots.py $(BUILD)/ow-$*.map $(OWOBJ)
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"

$(DIST)/ow-data.prg: $(OWENG)
	cmp $(BUILD)/ow-cpu.data $(BUILD)/ow-reu.data
	cp $(BUILD)/ow-cpu.data $@
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"

$(DIST)/splash-%.prg: $(SPLASHKIT)/data/splash-%.v2i $(SPLASHKIT)/data/splash-%.v2t \
                      tools/mksplash.py
	@mkdir -p $(DIST)
	tools/mksplash.py $(SPLASHKIT)/data/splash-$*.v2i $(SPLASHKIT)/data/splash-$*.v2t $@

$(DIST)/ow-save.prg: tools/mkowsave.py tools/isleref.py tools/mktowns.py
	@mkdir -p $(DIST)
	tools/mkowsave.py $@

# c1541 -format makes a fresh image over whatever was there. On the disk:
# the music, m.* (the bootstrap loads the tune a game starts with, an engine
# any other); the screens of text and the conversations (owtown, owtalk) and
# the talk's files (tools/mktalk.py); and the fights (src/lib/ow_fight.c):
# the module and its fields and sprites, as the arena's disk has them.
$(DIST)/openworld.d81: $(DIST)/openworld.prg $(OWENG) $(OWOVL) $(OWFILES) $(DIST)/owtown.prg $(DIST)/owtalk.prg \
	    $(DIST)/owparty.prg \
	    $(BUILD)/music/.stamp $(BUILD)/talk/.stamp $(DIST)/fight.prg $(DIST)/seafight.prg \
	    $(BUILD)/fight/disk/.stamp $(OWSETS)
	$(C1541) -format "openworld,01" d81 $@ >/dev/null
	$(C1541) -attach $@ -write $(DIST)/openworld.prg openworld \
	    -write $(DIST)/splash-p.prg splash-p -write $(DIST)/splash-n.prg splash-n \
	    -write $(DIST)/ow-save.prg ow-save -write $(DIST)/ow-data.prg ow-data \
	    -write $(DIST)/ow-isl.prg ow-isles \
	    -write $(DIST)/ow-cpu.prg ow-cpu -write $(DIST)/ow-reu.prg ow-reu \
	    $(foreach f,$(OWOVL),-write $(f) $(notdir $(f))) \
	    -write $(DIST)/owtown.prg owtown -write $(DIST)/owtalk.prg owtalk \
	    $$(for p in $(PORTRAITS); do n=$(call NPC_NUM,$$p); \
	        for s in n p; do printf ' -write %s v.%s%s' $(BUILD)/portraits/$$p-$$s $$n $$s; done; done) \
	    -write $(DIST)/owparty.prg owparty \
	    -write $(DIST)/fight.prg fight -write $(DIST)/seafight.prg seafight \
	    $$(for f in $(BUILD)/ow/sets/t.*; do printf ' -write %s %s' $$f $${f##*/}; done) \
	    $$(for f in $(BUILD)/fight/disk/[fs].*; do printf ' -write %s %s' $$f $${f##*/}; done) \
	    $$(for f in $(BUILD)/music/m.*; do printf ' -write %s %s' $$f $${f##*/}; done) \
	    $$(for f in $(BUILD)/talk/[0-9]*; do printf ' -write %s %s' $$f $${f##*/}; done) >/dev/null

-include $(foreach p,$(OWPARTS),$(BUILD)/ow/$(p)-cpu.d $(BUILD)/ow/$(p)-reu.d)
-include $(BUILD)/ow/ow_town_gen.d $(BUILD)/ow/ow_town_data.d $(BUILD)/ow/ow_town_maps.d

# The REU load cache (src/lib/ow_cache.s): every file the open world loads
# through it, in the order they get rooms in the REU (the most used first).
# tools/mkowcache.py makes the bootstrap's table of them (where each is in
# the REU's directory, where it loads, how long it is) and this build's
# stamp, and fails if two names share an entry; and the image of an REU
# with all of them in it, for the tests (screendump.py --reucache).
OWCACHE_FILES := $(DIST)/ow-data.prg=ow-data $(DIST)/ow-isl.prg=ow-isles \
    $(DIST)/ow-cpu.prg=ow-cpu $(DIST)/ow-reu.prg=ow-reu \
    $(foreach e,cpu reu,$(foreach o,hi tl world town dungeon,$(DIST)/ow-$(e).$(o)=ow-$(e).$(o))) \
    $(DIST)/owtown.prg=owtown $(DIST)/owtalk.prg=owtalk --glob '$(BUILD)/ow/sets/t.*' \
    $(DIST)/fight.prg=fight $(DIST)/seafight.prg=seafight --glob '$(BUILD)/music/m.*' \
    --glob '$(BUILD)/fight/disk/[fs].*' $(DIST)/owparty.prg=owparty \
    --glob '$(BUILD)/talk/[0-9]*'
OWCACHE_DEPS := $(DIST)/ow-data.prg $(DIST)/ow-isl.prg $(OWENG) $(OWOVL) $(DIST)/owtown.prg $(DIST)/owtalk.prg \
    $(DIST)/fight.prg $(DIST)/seafight.prg $(DIST)/owparty.prg $(OWSETS) $(BUILD)/music/.stamp \
    $(BUILD)/fight/disk/.stamp $(BUILD)/talk/.stamp tools/mkowcache.py include/q4k/owcache.inc

$(BUILD)/ow/owcdir.s: $(OWCACHE_DEPS)
	@mkdir -p $(@D)
	tools/mkowcache.py dir $@ $(OWCACHE_FILES)

$(BUILD)/ow/owcdir.o: $(BUILD)/ow/owcdir.s
	$(CL65) $(ASFLAGS) -c -o $@ $<

$(BUILD)/owcache.reu: $(OWCACHE_DEPS)
	tools/mkowcache.py image $@ 512 $(OWCACHE_FILES)

# The screens of text, owtown (shops, inns, the party, help, a fight) and
# owtalk (a conversation), with the few variables and functions the game's
# modules read and write (src/ow/owstand.c), which the engine defines for
# itself. isletest, the generators' check, links it too.
$(BUILD)/ow/owstand.o: src/ow/owstand.c
	@mkdir -p $(dir $@)
	cc65 $(CFLAGS) --create-full-dep $(@:.o=.d) --dep-target $@ -o $(@:.o=.s) $<
	ca65 -t c64 -o $@ $(@:.o=.s)
-include $(BUILD)/ow/owstand.d

$(DIST)/owtown.prg $(DIST)/owtalk.prg $(DIST)/isletest.prg: $(DIST)/%.prg: $(BUILD)/%.o $(BUILD)/ow/owstand.o $(LIB) $(wildcard cfg/*.cfg)
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C $(LDCFG) -m $(BUILD)/$*.map \
	    -Wl -Ln,$(BUILD)/$*.lbl -o $@ $< $(EXTRA_$*) $(BUILD)/ow/owstand.o $(LIB)
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"

# owtalk's portraits (src/ow/talkpic.c): its own talk screen, and vic2orious's
# v2i.inc and v2t.inc (assets/vic2/lib, version 6) round picshow.s, -t none
# as the splash's are. Each portrait in assets/portraits is NAME-n.v2i and
# NAME-p.v2i (NTSC and PAL), NAME a person's id in towns.txt; the disk has
# it as v.NUMBERn and v.NUMBERp, NUMBER their NPC_* in town_data.h, which
# tools/mktowns.py numbers in the order the maps meet them.
EXTRA_owtalk := $(BUILD)/ow/talkpic.o $(BUILD)/ow/picshow.o
$(DIST)/owtalk.prg: $(EXTRA_owtalk)
$(BUILD)/ow/talkpic.o: src/ow/talkpic.c include/q4k/town_data.h Makefile
	@mkdir -p $(dir $@)
	pics=0; for p in $(PORTRAITS); do n=$(call NPC_NUM,$$p); \
	    pics=$$((pics | 1 << n)); done; \
	cc65 $(CFLAGS) -DPICS=$$pics --create-full-dep $(@:.o=.d) --dep-target $@ -o $(@:.o=.s) $<
	ca65 -t c64 -o $@ $(@:.o=.s)
-include $(BUILD)/ow/talkpic.d
$(BUILD)/ow/picshow.o: src/ow/picshow.s src/ow/vic2/font.inc assets/vic2/lib/v2i.inc assets/vic2/lib/v2t.inc
	@mkdir -p $(dir $@)
	ca65 -t none -I assets/vic2/lib -I src/ow/vic2 -o $@ $<

PORTRAITS := tam princess king
# A person's NPC_* number, in a recipe's shell: $(call NPC_NUM,ID).
NPC_NUM = $$(n=$$(sed -n "s/^.define NPC_$$(echo $(1) | tr a-z A-Z)  *\([0-9]*\).*/\1/p" \
    include/q4k/town_data.h); [ -n "$$n" ] || { echo "portrait $(1): no NPC_ in town_data.h" >&2; \
    exit 1; }; echo $$n)
PORTRAIT_FILES := $(foreach p,$(PORTRAITS),$(foreach s,n p,$(BUILD)/portraits/$(p)-$(s)))
$(BUILD)/portraits/%: assets/portraits/%.v2i tools/mkportrait.py
	@mkdir -p $(dir $@)
	tools/mkportrait.py $< $@
$(DIST)/openworld.d81: $(PORTRAIT_FILES) include/q4k/town_data.h

# The bootstrap, with the cache's table.
$(DIST)/openworld.prg: $(BUILD)/openworld.o $(BUILD)/ow/owcdir.o $(LIB) $(wildcard cfg/*.cfg)
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C cfg/c64-owboot.cfg -m $(BUILD)/openworld.map \
	    -Wl -Ln,$(BUILD)/openworld.lbl -o $@ $< $(BUILD)/ow/owcdir.o $(LIB)
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"

# The multicolour arena (src/owarena.c) and the fight it calls: a module
# linked on its own (src/fight/, cfg/c64-fight.cfg, include/q4k/fight.h),
# with the party's arms out of the library. Its disk holds the arena, the
# module, and what the module loads for a fight: a field (f.*) and the
# monster's sprites (s.*), which tools/mkfields.py makes. The music and the
# effects are the open world's (ow_tune.s from the library, and ow_sfx.o,
# ahead of the library's sfx.s), called from the fight's interrupt.
FIGHTOBJ := $(patsubst %,$(BUILD)/fight/%.o,head block fight mux fight_data corners) \
            $(BUILD)/ow/ow_sfx.o
FIGHT_STAMP := $(BUILD)/fight/disk/.stamp

# -Or (SMALLMODS): a tenth smaller. A static pattern rule, naming its
# targets: as a plain one it also matched $(BUILD)/%.o: src/%.c above, and
# GNU make 3.81 (macOS's /usr/bin/make) takes the first rule that matches,
# not the most specific as 3.82 and later do, so it built fight.c with
# -Osir, 329 bytes bigger.
$(BUILD)/fight/block.o $(BUILD)/fight/fight.o: $(BUILD)/fight/%.o: src/fight/%.c
	@mkdir -p $(dir $@)
	$(CL65) $(filter-out -Osir,$(CFLAGS)) -Or $(DEPFLAGS) -c -o $@ $<

$(BUILD)/fight/%.o: src/fight/%.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) -c -o $@ $<

$(DIST)/fight.prg: $(FIGHTOBJ) $(LIB) cfg/c64-fight.cfg
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C cfg/c64-fight.cfg -m $(BUILD)/fight.map \
	    -Wl -Ln,$(BUILD)/fight.lbl -o $@ $(FIGHTOBJ) $(LIB)
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"

# The module again with its autoplayer (FIGHT_BOT), which the arena's `*`
# loads for tools/balance.py --ow. After fight.o, never beside it: cl65
# writes src/fight/fight.s on the way to either.
FIGHTBOTOBJ := $(subst $(BUILD)/fight/fight.o,$(BUILD)/fight/fight_bot.o,$(FIGHTOBJ))

$(BUILD)/fight/fight_bot.o: src/fight/fight.c | $(BUILD)/fight/fight.o
	@mkdir -p $(dir $@)
	$(CL65) $(filter-out -Osir,$(CFLAGS)) -Or -DFIGHT_BOT $(DEPFLAGS) -c -o $@ $<

$(DIST)/fightbot.prg: $(FIGHTBOTOBJ) $(LIB) cfg/c64-fight.cfg
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C cfg/c64-fight.cfg -m $(BUILD)/fightbot.map \
	    -Wl -Ln,$(BUILD)/fightbot.lbl -o $@ $(FIGHTBOTOBJ) $(LIB)
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"

# And again for a fight at sea (SEA_FIGHT): on the deck of the king's ship,
# with monsters that keep to the water (ow_fight.c's OWF_SEA, f.deck).
# After fight_bot.o, for the same reason.
SEAFIGHTOBJ := $(subst $(BUILD)/fight/fight.o,$(BUILD)/fight/fight_sea.o,$(FIGHTOBJ))

$(BUILD)/fight/fight_sea.o: src/fight/fight.c | $(BUILD)/fight/fight_bot.o
	@mkdir -p $(dir $@)
	$(CL65) $(filter-out -Osir,$(CFLAGS)) -Or -DSEA_FIGHT $(DEPFLAGS) -c -o $@ $<

$(DIST)/seafight.prg: $(SEAFIGHTOBJ) $(LIB) cfg/c64-fight.cfg
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C cfg/c64-fight.cfg -m $(BUILD)/seafight.map \
	    -Wl -Ln,$(BUILD)/seafight.lbl -o $@ $(SEAFIGHTOBJ) $(LIB)
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"

$(FIGHT_STAMP): $(FIELD_SRC)
	@rm -rf $(BUILD)/fight/disk
	tools/mkfields.py $(BUILD)/fight/disk
	@touch $@

$(DIST)/owarena.d64: $(DIST)/owarena.prg $(DIST)/fight.prg $(DIST)/fightbot.prg \
	    $(FIGHT_STAMP)
	$(C1541) -format "owarena,01" d64 $@ >/dev/null
	$(C1541) -attach $@ -write $(DIST)/owarena.prg owarena \
	    -write $(DIST)/fight.prg fight -write $(DIST)/fightbot.prg fightbot \
	    $$(for f in $(BUILD)/fight/disk/[fs].*; do printf ' -write %s %s' $$f $${f##*/}; done) >/dev/null

-include $(BUILD)/fight/fight.d $(BUILD)/fight/fight_bot.d $(BUILD)/fight/fight_sea.d \
    $(BUILD)/fight/block.d

# The open world's memory map (docs/memmap.html, checked in): where every
# byte goes in each of its modes, from the linker maps the programs below
# leave in $(BUILD), the disk's data files, the game block's size and the
# engine's sprites. tools/mkmemmap.py stamps it with the date and the commit,
# and rewrites it only when an address moved, so a relink that moves nothing
# leaves it as it was.
ifneq ($(MEMMAP),)
$(MEMMAP): tools/mkmemmap.py tools/memmap-template.html \
	    $(DIST)/openworld.prg $(DIST)/owparty.prg $(OWENG) $(DIST)/owtown.prg $(DIST)/owtalk.prg \
	    $(DIST)/fight.prg $(DIST)/ow-data.prg $(DIST)/splash-p.prg \
	    $(DIST)/splash-n.prg $(DIST)/theme.sid $(FIGHT_STAMP) \
	    include/q4k/owgame.h src/lib/ow_data.s $(PORTRAIT_FILES)
	tools/mkmemmap.py $@
endif

# The music: SID files, one per tune, loaded when a scene wants them
# (assets/music.txt, play_for() in src/ow/engine.c). The theme is built here, its player
# (src/sid/theme.s) linked at $E800 with cfg/sid.cfg and wrapped as a PSID,
# which also lands in $(DIST) to play in any SID player; the rest are
# assets/music/*.sid. tools/mksid.py checks each and writes the disk's
# m.NAME files into $(BUILD)/music.
SIDDIR := $(BUILD)/sid
SIDS   := $(SIDDIR)/theme.sid

$(SIDDIR)/theme.bin: src/sid/theme.s $(GEN_MUSIC) cfg/sid.cfg
	@mkdir -p $(SIDDIR)
	$(CL65) -t none -C cfg/sid.cfg -Ln $(SIDDIR)/theme.lbl -m $(SIDDIR)/theme.map \
	    -o $@ src/sid/theme.s $(GEN_MUSIC)

$(SIDDIR)/theme.sid: $(SIDDIR)/theme.bin tools/mksid.py
	tools/mksid.py wrap $< $(SIDDIR)/theme.lbl $@

$(DIST)/theme.sid: $(SIDDIR)/theme.sid
	@mkdir -p $(DIST)
	cp $< $@

MUSIC_STAMP := $(BUILD)/music/.stamp

$(MUSIC_STAMP): assets/music.txt $(SIDS) $(wildcard assets/music/*.sid) \
                tools/mksid.py tools/sid6502.py
	tools/mksid.py disk $(BUILD)/music $(SIDDIR) assets/music

TALK_STAMP := $(BUILD)/talk/.stamp

$(TALK_STAMP): assets/talk.txt assets/hints.json tools/mktalk.py \
               include/q4k/town_data.h src/lib/town_data.c
	@rm -rf $(BUILD)/talk
	tools/mktalk.py $(BUILD)/talk $(TALK_SIZE)
	@touch $@

# --- run / verify -----------------------------------------------------------

# Both run from their disks: the open world's bootstrap loads an engine, the
# arena its fight.
run: all
	tools/run.sh $(DIST)/$(PROG).$(if $(filter openworld,$(PROG)),d81,d64)

run-reu: all
	tools/run.sh $(DIST)/$(PROG).$(if $(filter openworld,$(PROG)),d81,d64) -reu -reusize 512

# Play-test one town of assets/towns.txt (TOWN=its id, or gen for the town
# the game makes on home), from a copy of the disk whose saved game is a
# lone warrior inside it, at the map's $ or at AT=x,y: press F3 at the
# title. Leaving puts him on home outside that town (Millford's square for
# one across the sea). F1 saves to the copy, never to dist/openworld.d81.
TOWN ?= millford
playtown: all
	cp $(DIST)/openworld.d81 $(BUILD)/playtown.d81
	tools/mkowsave.py $(BUILD)/playtown-save.prg --town $(TOWN) $(if $(AT),--at $(AT))
	$(C1541) -attach $(BUILD)/playtown.d81 -delete ow-save \
	    -write $(BUILD)/playtown-save.prg ow-save >/dev/null
	@echo "playtown: $(TOWN) -- press F3 at the title"
	tools/run.sh $(BUILD)/playtown.d81

# Compare each case's screen against tests/golden/<name>.txt, several at a
# time. TESTARGS go to tools/test.sh: -j 1 for one at a time, --group
# goldens|cases for half of it (CI's split), or names of units.
test: all
	@tools/test.sh $(TESTARGS)

# Accept the current output as correct. Review the diff before doing this.
bless: all
	@tools/test.sh --bless $(TESTARGS)

clean:
	rm -rf $(BUILD) $(DIST)
