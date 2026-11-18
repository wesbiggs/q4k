# q4k -- Commodore 64 build
#
# Layout:
#   src/<name>.c    a program with main(); builds to dist/<name>.prg
#   src/lib/*.c|*.s shared modules; archived into a .lib so the linker
#                   pulls in only what each program actually references
#
# Usage:
#   make            build every program; the game is the offline build
#   make run        launch PROG in VICE  (make run PROG=hello)
#   make check      headless run, dump the C64 screen as text
#   make test       headless run, diff every program against its golden
#   make bless      accept current output as the new goldens
#   make assets     regenerate the C and JSON built from assets/
#   make shot       headless run, save a PNG screenshot
#   make d64        build a bootable disk image (the game's, by default)
#   make online     the experimental networked game, in dist/online/
#   make d64-online    ... on its disk
#   make play          ... and play it with the NPC dialogue daemon attached
#   make release    the files a release publishes, in dist/release/
#   make clean

TARGET   := c64
PROG     ?= hello

BUILD    := build
DIST     := dist

# The game is the offline build: no modem and no host. What people say is on
# the disk, a file each, which src/lib/talk.c loads into the talk's buffer
# (tools/mktalk.py makes them from assets/talk.txt, and the inns' rumours from
# assets/hints.json).
#
# The networked build, experimental, talks to a host over the modem instead
# (ONLINE=1, -DQ4K_ONLINE). Everything of it goes in a directory of its own,
# so the two builds' objects never mix; use the targets at the end, which set
# ONLINE.
ONLINE   ?= 0
ifeq ($(ONLINE),1)
BUILD    := build/online
DIST     := dist/online
endif

# The talk's buffer, at the end of its engine (src/lib/talkbuf.s): offline,
# one person's lines or one inn's rumours. mktalk.py checks every file
# against it.
TALK_SIZE := 0x0BEC

CL65     := cl65
AR65     := ar65
C1541    := c1541

# -Osir: optimize, inline known routines, use register vars where it pays.
# --static-locals: locals become static storage -- faster on 6502 (no stack
# frame juggling) at the cost of no recursion. Right trade for a tile RPG.
OPT      := -Osir
CFLAGS    = -t $(TARGET) $(OPT) --static-locals -I include \
            $(if $(filter 1,$(ONLINE)),-DQ4K_ONLINE)
ASFLAGS  := -t $(TARGET) -I include --asm-include-dir include \
            --asm-define TALK_SIZE=$(TALK_SIZE)
# openworld, the smooth-scrolling prototype, has a map of its own
# (cfg/c64-openworld.cfg), with VIC bank 1 kept for its screens.
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
# The game loads its engines from the disk (cfg/c64-q4k.cfg, and its own
# rule below). dungeontest links the maps and the font, arena the fights and
# isletest the land, so they need the game's charset and probe page as well,
# with everything in one load (cfg/c64-flat.cfg).
LDCFG     = cfg/c64-$(if $(filter dungeontest arena isletest,$*),flat,$(if $(filter owtown,$*),owtown,$(if $(filter openworld,$*),owboot,$(if $(filter owarena,$*),owarena,tool)))).cfg

MAINS    := $(wildcard src/*.c)
PRGS     := $(patsubst src/%.c,$(DIST)/%.prg,$(MAINS))

# The core's modules whose code and constants live above the character set,
# in the HIGH region, rather than in MAIN. Both flags are needed: --code-name
# alone leaves the string literals behind. (A module in an engine, below, goes
# to its engine instead: being on this list as well would change nothing.)
HIMODS   := arms panel savegame sprite world modem line actor quest

# These are compiled without -Os, which unrolls shifts and multiplies by
# constants and costs about a tenth of their size: the screens lose nothing
# by it, and the fight (battle) is turn-based, so a flood fill a few
# milliseconds slower goes unnoticed. Not the generators or the modem, whose
# timing was measured with it. Changing this list needs the objects rebuilt,
# as above.
SMALLMODS  := loot shop inn help battle arms dialog dungeon talk gift
$(foreach m,$(SMALLMODS),$(eval $(BUILD)/lib/$(m).o: OPT := -Or))

# The title screen is not in HIGH at all: it runs once, at the start, from the
# engines' slot, which the PRG brings it in (cfg/c64-q4k.cfg) ...
$(BUILD)/lib/title.o: CFLAGS += --code-name TITLECODE --rodata-name TITLEDATA
# ... and so is what draws the game's character set, which is done before the
# map's engine is loaded over it: charset_define.o here, and the glyphs in
# tiles_data.c (see tools/mktiles.py, which marks them).
$(BUILD)/lib/charset_define.o: CFLAGS += --code-name TITLECODE

LIBSRC   := $(wildcard src/lib/*.c) $(wildcard src/lib/*.s)
LIBOBJ   := $(patsubst src/lib/%.c,$(BUILD)/lib/%.o,$(wildcard src/lib/*.c)) \
            $(patsubst src/lib/%.s,$(BUILD)/lib/%.o,$(wildcard src/lib/*.s))

# What talks is the only difference between the two builds: the offline one
# from the disk (talk, and its buffer), the networked one over the modem
# (dialog, with modem and line in the core). Never both. The modem driver
# stays in the offline build's library, for serialtest: the game never calls
# it, so it never links it.
OFFONLY    := talk talkbuf
ONLINEONLY := dialog line
ifeq ($(ONLINE),1)
LIBOBJ   := $(filter-out $(patsubst %,$(BUILD)/lib/%.o,$(OFFONLY)),$(LIBOBJ))
else
LIBOBJ   := $(filter-out $(patsubst %,$(BUILD)/lib/%.o,$(ONLINEONLY)),$(LIBOBJ))
endif

# The engines (include/q4k/engine.h, cfg/c64-q4k.cfg): what is loaded into the
# slot for each, and so not in memory otherwise. Their code and constants go
# in the engine's segments, and their variables stay in the core's BSS and
# DATA, so that nothing they keep is lost when another engine takes the slot.
# Everything else is the core. The assembly modules choose their own
# segments: isle_gen, isle_rnd, tgen and place_near are the map's, open9 the fight's,
# overlay has the fight's arrays and talkbuf the talk's buffer; town_data puts
# the people's ids, which only a conversation needs, with the talk.
# tools/mkengines.py checks after every link that no engine calls another,
# nor the core an engine.
ENG_MAP   := scene draw fov town_data town_gen loot loot_data sail \
             dungeon fightcall
ENG_FIGHT := battle bestiary arena_data
ENG_TALK  := talk dialog inn gift
ENG_SHOP  := shop help
ENGMODS   := $(ENG_MAP) $(ENG_FIGHT) $(ENG_TALK) $(ENG_SHOP)

$(foreach e,MAP FIGHT TALK SHOP,$(foreach m,$(ENG_$(e)),$(eval $(BUILD)/lib/$(m).o: CFLAGS += --code-name $(e)CODE --rodata-name $(e)DATA)))
# The map loop itself, in src/q4k.c: its main() is boot.c's.
$(BUILD)/q4k.o: CFLAGS += --code-name MAPCODE --rodata-name MAPDATA
$(foreach m,$(filter-out $(ENGMODS),$(HIMODS)),$(eval $(BUILD)/lib/$(m).o: CFLAGS += --code-name HICODE --rodata-name HIDATA))

ifneq ($(strip $(LIBOBJ)),)
LIB      := $(BUILD)/q4k.lib
endif

.PHONY: all run play check shot test bless d64 release clean dirs assets \
        online d64-online run-online
.SECONDARY: $(BUILD)/$(PROG).o $(BUILD)/q4k.o

# The open world's memory map, a page drawn from the links below (offline
# only: the online build's open world is the same).
MEMMAP := $(if $(filter 1,$(ONLINE)),,docs/memmap.html)

all: dirs assets $(PRGS) $(DIST)/openworld.d81 $(DIST)/owarena.d64 \
     $(DIST)/theme.sid $(BUILD)/music/.stamp $(MEMMAP)

# Assembly cannot see tiles.h; it has its own generated copy of the codes.
$(BUILD)/lib/isle_gen.o: include/q4k/tiles.inc

# --- generated from assets ---------------------------------------------------
#
# The generated C and the geography the NPCs are given are checked in, so a
# plain build needs no Python. These rules keep them from going stale: edit
# an asset and the next make regenerates whatever depended on it.

GEN_TILES := include/q4k/tiles.h include/q4k/tiles.inc src/lib/tiles_data.c \
            src/lib/tiles_names.c
GEN_TOWN  := include/q4k/town_data.h src/lib/town_data.c
GEN_ARENA := include/q4k/arena_data.h src/lib/arena_data.c
GEN_NAMES := include/q4k/names_data.h
GEN_GEO   := assets/geography.json
GEN_FONT  := src/lib/font_data.c
GEN_LOOT  := include/q4k/loot_data.h src/lib/loot_data.c
GEN_VER   := include/q4k/version.h
GEN_MUSIC := src/sid/theme_data.s
GEN_TUNES := include/q4k/music_map.h src/lib/music_map.c
GEN_OW    := src/lib/ow_data.s src/lib/ow_figs.s
GEN_FIGHT := src/fight/fight_data.s src/ow/foe_art.s

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

assets: $(GEN_TILES) $(GEN_TOWN) $(GEN_ARENA) $(GEN_NAMES) $(GEN_GEO) $(GEN_FONT) \
        $(GEN_LOOT) $(GEN_VER) $(GEN_MUSIC) $(GEN_TUNES) $(GEN_OW) $(GEN_FIGHT)

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

$(GEN_TOWN): assets/towns.txt assets/tiles.txt assets/npcs.json tools/mktowns.py \
             tools/rle.py
	tools/mktowns.py

$(GEN_ARENA): assets/arenas.txt assets/tiles.txt tools/mkarenas.py \
              tools/mktiles.py tools/checkmap.py tools/rle.py
	tools/mkarenas.py

$(GEN_LOOT): assets/loot.txt assets/towns.txt \
             assets/tiles.txt assets/npcs.json tools/mkloot.py tools/mktowns.py \
             tools/mktiles.py
	tools/mkloot.py

$(GEN_MUSIC): assets/theme.txt tools/mkmusic.py
	tools/mkmusic.py

$(GEN_TUNES): assets/music.txt tools/mksid.py
	tools/mksid.py map

$(GEN_OW): assets/overworld.txt $(wildcard assets/*_tiles.txt) \
           assets/places_common.txt assets/tiles.txt assets/monsters.txt \
           tools/mkoverworld.py tools/figures.py
	tools/mkoverworld.py

# The open world's tile sets, a file each on its disk (t.overworld ...),
# which the engine loads into SHORE on the way into an area.
OWSETS := $(BUILD)/ow/sets/.stamp

$(OWSETS): assets/overworld.txt $(wildcard assets/*_tiles.txt) \
           assets/places_common.txt assets/tiles.txt assets/monsters.txt \
           tools/mkoverworld.py tools/figures.py
	@mkdir -p $(@D)
	tools/mkoverworld.py --sets $(@D)
	@touch $@

# The multicolour arena's sprites (the ones linked into the fight), and
# further down the files of its disk: the fields, drawn with the prototype's
# tile sets, and each monster's sprites.
FIELD_SRC := assets/fields.txt assets/monsters.txt assets/field_tiles.txt \
             assets/overworld.txt assets/dungeon_tiles.txt \
             assets/places_common.txt assets/tiles.txt tools/mkfields.py \
             tools/mkoverworld.py tools/figures.py

$(GEN_FIGHT): $(FIELD_SRC)
	tools/mkfields.py

$(GEN_NAMES): assets/names.txt tools/mknames.py
	tools/mknames.py

$(GEN_GEO): assets/towns.txt assets/places.txt assets/names.txt \
            assets/tiles.txt tools/mkgeo.py tools/mknames.py
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

# The arena plays itself (src/arena.c): battle.c again, with the fight's
# autoplayer compiled in (FIGHT_BOT), linked ahead of the library so that its
# fight is the one the arena gets. Nothing else links it. After battle.o, never
# beside it: cl65 writes src/lib/battle.s on the way to either, and two at
# once tread on it.
$(BUILD)/lib/battle_bot.o: OPT := -Or
$(BUILD)/lib/battle_bot.o: src/lib/battle.c | $(BUILD)/lib/battle.o
	@mkdir -p $(dir $@)
	$(CL65) $(CFLAGS) -DFIGHT_BOT --code-name FIGHTCODE --rodata-name FIGHTDATA \
	    $(DEPFLAGS) -c -o $@ $<

$(DIST)/arena.prg: $(BUILD)/arena.o $(BUILD)/lib/battle_bot.o $(LIB) $(wildcard cfg/*.cfg)
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C cfg/c64-flat.cfg -m $(BUILD)/arena.map \
	    -Wl -Ln,$(BUILD)/arena.lbl -o $@ $< $(BUILD)/lib/battle_bot.o $(LIB)
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
OWOVL := $(foreach e,cpu reu,$(foreach o,world town dungeon hi,$(DIST)/ow-$(e).$(o)))
OWFILES := $(DIST)/ow-data.prg $(DIST)/splash-p.prg $(DIST)/splash-n.prg \
           $(DIST)/ow-save.prg

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

# The dungeon generator, src/lib/dungeon.c, again, in the dungeon's overlay:
# the library's copy is the game's map engine's (MAPCODE), which here is the
# core. Linked ahead of the library, so the library's is not pulled in. Its
# constants stay in COMMON (MAPDATA), the same bytes in both engines. After
# the library's, never beside it: cl65 writes src/lib/dungeon.s on the way
# to either.
$(BUILD)/ow/ow_dungeon.o: src/lib/dungeon.c | $(BUILD)/lib/dungeon.o
	@mkdir -p $(dir $@)
	$(CL65) $(filter-out -Osir,$(CFLAGS)) -Or --code-name OVDCODE \
	    --rodata-name MAPDATA $(DEPFLAGS) -c -o $@ $<

# The generators the open world builds with, in the overlay that uses them:
# the land's (isle_gen.s, OW_WORLD) and the build's thread (genbuild.s) in
# the world's, the towns' (tgen.s, OW_TOWN, town_gen.c, and town_data.c's
# maps) in the town's. Linked ahead of the library, as ow_dungeon.o is, so
# that its MAPCODE copies are not; tools/owslots.py knows them by name.
# The C ones through build/ow/NAME.s, never src/lib/NAME.s (cl65 writes
# that for the library's).
$(BUILD)/ow/ow_isle.o: src/lib/isle_gen.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) --asm-define OW_WORLD=1 -c -o $@ $<
$(BUILD)/ow/ow_tgen.o: src/lib/tgen.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) --asm-define OW_TOWN=1 -c -o $@ $<
$(BUILD)/ow/ow_town_gen.o $(BUILD)/ow/ow_town_data.o: $(BUILD)/ow/ow_%.o: src/lib/%.c
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
# square by square (foes.s), in its overlay.
$(BUILD)/ow/foe_art.o $(BUILD)/ow/foes.o $(BUILD)/ow/stlow.o $(BUILD)/ow/genbuild.o: $(BUILD)/ow/%.o: src/ow/%.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) -c -o $@ $<

# The sound effects (src/lib/sfx.s) again, in the engine's own segments and
# ticking with its music: linked ahead of the library, as ow_dungeon.o is.
$(BUILD)/ow/ow_sfx.o: src/lib/sfx.s
	@mkdir -p $(dir $@)
	$(CL65) $(ASFLAGS) --asm-define OW_SFX=1 -c -o $@ $<

OWGEN := $(patsubst %,$(BUILD)/ow/%.o,ow_isle genbuild ow_tgen ow_town_gen ow_town_data)
OWOBJ = $(foreach p,$(OWPARTS) smooth,$(BUILD)/ow/$(p)-$*.o) $(BUILD)/ow/ovl.o \
        $(BUILD)/ow/ow_dungeon.o $(BUILD)/ow/ow_sfx.o $(BUILD)/ow/foe_art.o \
        $(BUILD)/ow/foes.o $(BUILD)/ow/stlow.o $(OWGEN)

# The overlays come out of the same link (ld65's %O.world, ...).
$(DIST)/ow-%.world $(DIST)/ow-%.town $(DIST)/ow-%.dungeon $(DIST)/ow-%.hi: $(DIST)/ow-%.prg ;

$(DIST)/ow-%.prg: $(foreach p,$(OWPARTS) smooth,$(BUILD)/ow/$(p)-%.o) $(BUILD)/ow/ovl.o \
	    $(BUILD)/ow/ow_dungeon.o $(BUILD)/ow/ow_sfx.o $(BUILD)/ow/foe_art.o $(BUILD)/ow/foes.o \
	    $(BUILD)/ow/stlow.o $(OWGEN) $(LIB) cfg/c64-openworld.cfg tools/owslots.py
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C cfg/c64-openworld.cfg -m $(BUILD)/ow-$*.map \
	    -Wl -Ln,$(BUILD)/ow-$*.lbl -o $@ $(OWOBJ) $(LIB)
	@mv $@.data $(BUILD)/ow-$*.data
	@mv $@.world $(DIST)/ow-$*.world
	@mv $@.town $(DIST)/ow-$*.town
	@mv $@.dungeon $(DIST)/ow-$*.dungeon
	@mv $@.hi $(DIST)/ow-$*.hi
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

$(DIST)/ow-save.prg: tools/mkowsave.py tools/isleref.py
	@mkdir -p $(DIST)
	tools/mkowsave.py $@

# c1541 -format makes a fresh image over whatever was there.
# (and the game's music, m.*: the bootstrap loads the tune a game starts
# with, an engine any other. MUSIC_STAMP is set further down, with the
# game's own disk, so its file is spelled out here.)
# The conversations (src/owtown.c, which the engine loads when the hero walks
# into a townsperson): the program, and the talk's files, which tools/mktalk.py
# makes for the game's disk too.
# And the fights (src/lib/ow_fight.c): the module and its fields and sprites,
# as the arena's disk has them.
$(DIST)/openworld.d81: $(DIST)/openworld.prg $(OWENG) $(OWOVL) $(OWFILES) $(DIST)/owtown.prg \
	    $(DIST)/owparty.prg \
	    $(BUILD)/music/.stamp $(BUILD)/talk/.stamp $(DIST)/fight.prg \
	    $(BUILD)/fight/disk/.stamp $(OWSETS)
	$(C1541) -format "openworld,01" d81 $@ >/dev/null
	$(C1541) -attach $@ -write $(DIST)/openworld.prg openworld \
	    -write $(DIST)/splash-p.prg splash-p -write $(DIST)/splash-n.prg splash-n \
	    -write $(DIST)/ow-save.prg ow-save -write $(DIST)/ow-data.prg ow-data \
	    -write $(DIST)/ow-cpu.prg ow-cpu -write $(DIST)/ow-reu.prg ow-reu \
	    $(foreach f,$(OWOVL),-write $(f) $(notdir $(f))) \
	    -write $(DIST)/owtown.prg owtown -write $(DIST)/owparty.prg owparty \
	    -write $(DIST)/fight.prg fight \
	    $$(for f in $(BUILD)/ow/sets/t.*; do printf ' -write %s %s' $$f $${f##*/}; done) \
	    $$(for f in $(BUILD)/fight/disk/[fs].*; do printf ' -write %s %s' $$f $${f##*/}; done) \
	    $$(for f in $(BUILD)/music/m.*; do printf ' -write %s %s' $$f $${f##*/}; done) \
	    $$(for f in $(BUILD)/talk/[0-9]*; do printf ' -write %s %s' $$f $${f##*/}; done) >/dev/null

-include $(foreach p,$(OWPARTS),$(BUILD)/ow/$(p)-cpu.d $(BUILD)/ow/$(p)-reu.d)
-include $(BUILD)/ow/ow_town_gen.d $(BUILD)/ow/ow_town_data.d

# The multicolour arena (src/owarena.c) and the fight it calls: a module
# linked on its own (src/fight/, cfg/c64-fight.cfg, include/q4k/fight.h),
# with the game's arms out of the library. Its disk holds the arena, the
# module, and what the module loads for a fight: a field (f.*) and the
# monster's sprites (s.*), which tools/mkfields.py makes. The music and the
# effects are the open world's (ow_tune.s from the library, and ow_sfx.o,
# ahead of the library's sfx.s), called from the fight's interrupt.
FIGHTOBJ := $(patsubst %,$(BUILD)/fight/%.o,head block fight mux fight_data) \
            $(BUILD)/ow/ow_sfx.o
FIGHT_STAMP := $(BUILD)/fight/disk/.stamp

# -Or, as the game's own fight is (SMALLMODS): a tenth smaller.
$(BUILD)/fight/%.o: src/fight/%.c
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

-include $(BUILD)/fight/fight.d $(BUILD)/fight/fight_bot.d $(BUILD)/fight/block.d

# The open world's memory map (docs/memmap.html, checked in): where every
# byte goes in each of its modes, from the linker maps the programs below
# leave in $(BUILD), the disk's data files, the game block's size and the
# engine's sprites. tools/mkmemmap.py stamps it with the date and the commit,
# and rewrites it only when an address moved, so a relink that moves nothing
# leaves it as it was.
ifneq ($(MEMMAP),)
$(MEMMAP): tools/mkmemmap.py tools/memmap.html \
	    $(DIST)/openworld.prg $(DIST)/owparty.prg $(OWENG) $(DIST)/owtown.prg \
	    $(DIST)/fight.prg $(DIST)/ow-data.prg $(DIST)/splash-p.prg \
	    $(DIST)/splash-n.prg $(DIST)/theme.sid $(FIGHT_STAMP) \
	    include/q4k/owgame.h src/lib/ow_data.s
	tools/mkmemmap.py $@
endif

# The game. The engines come out of the same link, as files beside the PRG:
# tools/mkengines.py, which also checks that no engine reaches into another.
# The disk gets them all (d64, below); the game does not go past its title
# screen without them.
ENGFILES := e.map e.fight e.talk e.shop

# The music: SID files, one per tune, loaded when a scene wants them
# (assets/music.txt, src/lib/tune.c). The theme is built here, its player
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

$(DIST)/q4k.prg: $(BUILD)/q4k.o $(LIB) $(wildcard cfg/*.cfg) tools/mkengines.py
	@mkdir -p $(DIST)
	$(CL65) $(LDFLAGS) -C cfg/c64-q4k.cfg -m $(BUILD)/q4k.map \
	    -Wl -Ln,$(BUILD)/q4k.lbl -o $@ $< $(BUILD)/lib/boot.o $(LIB)
	@printf '%-14s %6d bytes\n' "$@" "$$(( $$(wc -c < $@) - 2 ))"
	tools/mkengines.py $(BUILD) $@

ifneq ($(ONLINE),1)
TALK_STAMP := $(BUILD)/talk/.stamp

$(TALK_STAMP): assets/talk.txt assets/hints.json tools/mktalk.py \
               include/q4k/town_data.h src/lib/town_data.c
	@rm -rf $(BUILD)/talk
	tools/mktalk.py $(BUILD)/talk $(TALK_SIZE)
	@touch $@
endif

# --- run / verify -----------------------------------------------------------

# The game needs its disk (engine.h), so it runs from that, and so does the
# smooth-scrolling prototype (its bootstrap loads an engine).
DISKPROGS := q4k openworld owarena
run: dirs $(DIST)/$(PROG).prg $(if $(filter q4k,$(PROG)),d64) \
     $(if $(filter openworld,$(PROG)),$(DIST)/openworld.d81) \
     $(if $(filter owarena,$(PROG)),$(DIST)/owarena.d64)
	tools/run.sh $(DIST)/$(PROG).$(if $(filter openworld,$(PROG)),d81,$(if $(filter $(DISKPROGS),$(PROG)),d64,prg))

# Play the networked build with NPC dialogue: starts host/npcd.py and wires
# it to the modem port. Q4K_MOCK=1 make play  uses canned replies instead of
# calling the model.
play: d64-online
	tools/play.sh

check: dirs $(DIST)/$(PROG).prg
	tools/screendump.py $(DIST)/$(PROG).prg

shot: dirs $(DIST)/$(PROG).prg
	tools/screenshot.sh $(DIST)/$(PROG).prg $(BUILD)/$(PROG).png
	@echo "wrote $(BUILD)/$(PROG).png"

# Compare each program's screen against tests/golden/<name>.txt.
test: all d64 d64-online
	@tools/test.sh

# Accept the current output as correct. Review the diff before doing this.
bless: all d64 d64-online
	@tools/test.sh --bless

# --- release ----------------------------------------------------------------
#
# What .github/workflows/release.yml publishes for a v* tag, and what a
# player downloads instead of building:
#
#   q4k-<version>.d64   the game on its disk, with what everyone says:
#                       LOAD"Q4K",8,1 then RUN; the same as `make d64`. There
#                       is no .prg on its own: the game loads its engines
#                       from the disk, and saves to it
#   q4k-<version>-online.d64
#                       the experimental networked build on its disk, which
#                       talks to a host over the modem; `make d64-online`
#   q4k-<version>-online.zip
#                       that disk plus the dialogue host and what it reads,
#                       laid out as in the repo, so tools/play.sh runs as is

VERSION_NUM := $(shell tr -d ' \n' < VERSION)
REL_NAME    := q4k-$(VERSION_NUM)
REL_DIR     := $(DIST)/release
REL_STAGE   := $(BUILD)/release/$(REL_NAME)-online
REL_FILES   := README.md host/npcd.py tools/play.sh tools/run.sh \
               assets/canon.md assets/npcs.json assets/secrets.json \
               assets/hints.json assets/geography.json

release: dirs d64 d64-online
	@rm -rf $(BUILD)/release $(REL_DIR)
	@mkdir -p $(REL_DIR) $(REL_STAGE)/dist/online
	cp $(DIST)/q4k.d64 $(REL_DIR)/$(REL_NAME).d64
	cp dist/online/q4k.d64 $(REL_DIR)/$(REL_NAME)-online.d64
	tar -cf - $(REL_FILES) | tar -xf - -C $(REL_STAGE)
	cp dist/online/q4k.d64 $(REL_STAGE)/dist/online/q4k.d64
	cd $(BUILD)/release && python3 -m zipfile -c $(CURDIR)/$(REL_DIR)/$(REL_NAME)-online.zip $(REL_NAME)-online
	@ls -l $(REL_DIR)

# --- disk image -------------------------------------------------------------

# The disk is also the save disk: the game saves to unit 8 and the image
# has room, so on a TeensyROM+ (or anywhere) one mounted .d64 is both. PROG=
# on the command line picks another program; the default is the game.
D64PROG = $(if $(filter command line,$(origin PROG)),$(PROG),q4k)

d64: dirs $(DIST)/$(D64PROG).prg $(TALK_STAMP) $(MUSIC_STAMP)
	@rm -f $(DIST)/$(D64PROG).d64
	$(C1541) -format "q4k $(VERSION_NUM),01" d64 $(DIST)/$(D64PROG).d64 >/dev/null
	$(C1541) -attach $(DIST)/$(D64PROG).d64 -write $(DIST)/$(D64PROG).prg $(D64PROG) >/dev/null
ifeq ($(D64PROG),q4k)
	$(C1541) -attach $(DIST)/$(D64PROG).d64 \
	    $$(for f in $(ENGFILES); do printf ' -write %s %s' $(DIST)/$$f $$f; done) \
	    $$(for f in $(BUILD)/music/m.*; do printf ' -write %s %s' $$f $${f##*/}; done) >/dev/null
endif
ifeq ($(D64PROG)$(ONLINE),q4k0)
	$(C1541) -attach $(DIST)/$(D64PROG).d64 \
	    $$(for f in $(BUILD)/talk/[0-9]*; do printf ' -write %s %s' $$f $${f##*/}; done) >/dev/null
endif
	@echo "wrote $(DIST)/$(D64PROG).d64   (LOAD\"$(D64PROG)\",8,1 then RUN; saves go to the same disk)"

# --- online -----------------------------------------------------------------
#
# The experimental networked game: the same game, talking to a host over the
# modem instead of reading its lines from the disk. Only the game is built:
# the small tools are the same either way.

online:
	$(MAKE) ONLINE=1 dirs assets dist/online/q4k.prg

d64-online:
	$(MAKE) ONLINE=1 assets d64

# With no host on the line: `make play` starts one.
run-online: d64-online
	tools/run.sh dist/online/q4k.d64

clean:
	rm -rf $(BUILD) $(DIST)
