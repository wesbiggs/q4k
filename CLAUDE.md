# Quest for the King — working notes

Grid-based RPG roguelike for the Commodore 64 (Ultima III lineage).
cc65 + VICE on macOS. See DEVELOPERS.md for the workflow and README.md for
the story and how to play.

## Where to look

This file is long; search it for the **bold** lead-ins named here.

- **Two games share the code.** The game proper (`q4k.prg`, `dist/q4k.d64`:
  sections Memory, Music, The land, The party, Testing) and the open world
  (`openworld.prg`, `dist/openworld.d81`), the smooth-scrolling one, which is
  where most work happens now even though its section still calls it a
  prototype. The open world borrows the game's modules but none of its
  memory map.
- **The open world**, in the order a session usually needs it: "The
  smooth-scrolling prototype" (the disk, the bootstrap, the world, streaming,
  the REU, places and roofs); "The open world's engine is a core and three
  overlays" (`src/ow/`, the slot, `ovl.s`'s jump table, what fits where);
  "The text window, and `L`" (the two lines under the map, `sm_say()`);
  "The music carries on"; "The screens of text are a program" (`owtown`:
  talk, shops, inns, party, help, and `ow_back()`); "Fights in the open
  world"; "The open world's game lives in a block" (`OwGame`, saves); "The
  story in the open world" (gates and refusals). Keys are listed at the top
  of `src/ow/engine.c`; how the screen is built is at the top of
  `src/ow/smooth.s`.
- **Space** is the usual constraint: free bytes per region are in each
  section ("Memory" for the game, the open world's "Memory" and the engine's
  overlay paragraph). Read the `.map` after a change; `make` prints
  `owslots:` and `mkengines:` lines with the slot sizes.
- **The open world's memory map** is a page, `docs/memmap.html` (checked
  in): every mode's blocks and what is free, drawn by `tools/mkmemmap.py`
  (what each block is) into `tools/memmap.html` (the page) from the
  linker maps, the disk's data files, `OWG_SIZE` and the engine's sprite
  count. `make` reruns it after any of those programs relinks; it stamps
  the date and the commit the build came from (and whether the tree had
  changes on top), refuses blocks that overlap, and rewrites the file only
  when an address moved, so the stamp is when the map last changed. A new
  segment or program is a line in `blocks()` there.
- **Tests**: "Testing" (twice: the rules, then the harness), and "Debugging
  a hang in VICE" for when a screen test just times out.

## Conventions

- C for game logic; assembly only where profiling says it matters.
- `--static-locals` is on: **no recursion**. Locals are static storage, which
  is much faster than a 6502 software stack.
- Shared code goes in `src/lib/` and is archived with `ar65`, so the linker
  pulls in only what each program references. Adding a module to `src/lib/`
  costs nothing in programs that don't call it.
- Anything with `main()` goes directly in `src/` and becomes its own `.prg`.

## Memory

Set up in `cfg/c64-q4k.cfg`, which **both builds** link with: the game
(offline) and the experimental networked one have the same layout and the same engines, and differ
only in who talks (see "The two builds" below). A core stays in memory and one
slot holds one engine at a time, loaded from the disk.

- `$0801-$37FF` **MAIN** -- the core's code and rodata, with the C stack at
  its top.
- `$3800-$3FFF` **charset**, built at runtime. Must be in VIC bank 0 and not
  at `$1000`/`$1800`, where the VIC sees character ROM whatever RAM holds.
  Its reverse-video half is mostly glyphs the game never draws, and
  `charset_install()` copies only the ones it does, so three runs of them are
  free for read-only data: `CHDATA1` (112 bytes, `$3C90`), `CHDATA2` (248,
  `$3D08`) and `CHDATA3` (448, `$3E40`), each a linker segment of its own. A
  module puts its rodata there with `#pragma rodata-name ("CHDATA3")` after
  its includes (inn, sprite and the lot are there, and so are a few strings of `q4k.c` and
  `help.c` that did not fit anywhere else, as `static const char[]` between
  two pragmas; the land and town generators' tables are in `CHDATA1`). They
  are the core's, so they are there whatever the slot holds; 9 bytes are free
  in `CHDATA1`, 123 in `CHDATA2` (157 online) and 206 in `CHDATA3`, which
  starts at `$3E58`, past the party's three class glyphs (`$C8-$CA`).
  `$A0` is the input cursor, `$C0-$C7`
  the bow shot's sprite block, `$C8-$CA` the warrior, archer and wizard, and
  `$80-$91` the actors: leave those alone,
  and check `assets/tiles.txt` before using any glyph above `$91` on screen.
- `$4000-$83FF` **the slot**: one engine at a time -- the map's (`e.map`),
  the fight's, the talk's (people and inns) or the shop's -- loaded by
  `engine_call()` (`src/lib/engine.c`, `include/q4k/engine.h`; ENGINES.md has
  the design and its choices). At the start it holds the title screen, which
  the PRG brings: `title.o`, `charset_define.o` and the glyph table with
  `tiles_install()` in `tiles_data.c` (marked by `tools/mktiles.py`), in the
  segments `TITLECODE`/`TITLEDATA`; `boot.c`'s `main()` runs it, then loads
  the map's engine over it for good. The Makefile's `ENG_*` lists say which
  modules are in which engine: only their code and constants move, while their
  BSS and DATA stay in the core. **An engine may call the core and itself,
  nothing else**; the core and other engines reach it only through its
  `*_entry`. `tools/mkengines.py` checks that after every link and fails the
  build otherwise. Scratch that lasts only while its engine runs follows its
  code: the fight's arrays (`overlay.s`, `FIGHTBSS`) and the offline talk's
  buffer (`talkbuf.s`, `TALKBSS`). The music is not in the slot: it is a SID
  file loaded under the KERNAL when a scene wants it (see Music). The game
  needs its disk, and asks for it back if an engine's load fails. The map's engine is
  the biggest, 15731 bytes of the slot's 17408, then the fight's (14800). The
  shop's engine also holds the screens the map opens that are not a place:
  the help screen and the party screen (`I`), through `shop_entry()` with
  `SHOP_HELP`/`SHOP_PARTY`; the quest's next-step table (`quest.c`) is in the
  core, since both the map and the help screen print it.
- `$8400-$C0FF` **HIGH** -- the rest of the core: `HIDATA` for bulk tables
  and `HICODE` for code that will not fit below. Core modules listed in the
  Makefile's `HIMODS` are compiled with `--code-name HICODE --rodata-name
  HIDATA`; an engine's modules go to their engine whatever that list says.
  Changing either needs the objects rebuilt, because a target-specific
  `CFLAGS` will not invalidate them on its own. About 6.0K are free in HIGH
  (5.3K online), 5.1K in MAIN (less the stack) and 257 bytes in BSS -- but
  **only 20 online**, so anything new that lives in BSS has to be paid for
  there first. The party is why: `Actor` is kept small (no level, glyph or
  `alive`; a fallen member is one with no health) because four are in
  memory and four more in the save's buffer. The Makefile's `SMALLMODS` (loot, shop, inn, help, battle, arms,
  dialog, dungeon, talk and gift) are built with `-Or`, not `-Osir`: without
  `-Os` cc65 stops inlining shifts and the like, which is about a tenth
  smaller. The fight is in it on the grounds that a turn-based flood fill can
  afford to be slower; nothing whose timing was measured is. Changing `cfg/`
  relinks everything.
- **The two builds.** The game is the offline build: `make`, in `build/`
  and `dist/`, links `talk.c` and `talkbuf.s`. The networked build, which is
  experimental (`make online`, `ONLINE=1`, `-DQ4K_ONLINE`, in `build/online/`
  and `dist/online/`), links `dialog.c` (in the talk's engine) and `line.c`
  and `modem.c` (in the core, because the title screen asks for the host
  before any engine is loaded) instead. Both are called the same way
  (`include/q4k/dialog.h`), and `Q4K_ONLINE` is tested only there and in
  `title.c`; keep it that way. Measure both builds after a change to either;
  `tools/screendump.py` finds `build/online/q4k.lbl` for
  `dist/online/q4k.prg`, and `tools/modes.py` groups either map by mode.
- **The test programs** that link the game's modules (dungeontest, arena,
  isletest) use the same objects, engine segments and all, and link with
  `cfg/c64-flat.cfg`, which puts every engine's segments in HIGH in one load.
  `cfg/c64-tool.cfg` is for the small tools.
- **The C stack is at the top of MAIN, not where the cfg used to say.** cc65's
  startup points `sp` at `__MAIN_START__ + __MAIN_SIZE__`, `$3800`, and it
  grows down through MAIN's spare bytes and then the dead ONCE
  segment. Painting `$3700-$37FF` and running a save and a long walk found
  the stack no deeper than 31 bytes, so what is left is plenty, but measure
  again before spending more of it.
- `$CF14-$CFFF` **BSS2**, 236 bytes of the probe page the probe does not use
  (it keeps `$CF00-$CF13`; `screendump.py` reads that much). Above the RS-232
  buffers, so it is not contiguous with BSS, and ld65 does not clear it: put
  something there with `#pragma bss-name (push, "BSS2")` only if it is always
  written before it is read. `fov_lit` (192 bytes), the fight's copy of a
  continent's monster, `native`, where each of the party stands in a fight
  and the foes' health are there; 5 bytes of BSS2 are left.
  Anything that needs the probe page longer must move the bound in both the
  cfg and `screendump.py`.
- `$C100-$CCFF` **BSS**, then the RS-232 buffers at `$CD00-$CEFF`
  (`modem_open()` raises `MEMSIZ` to `$CF00`, and the KERNAL takes them from
  just below it; the offline build keeps them free too, so that the layout is
  one file) and the probe page at `$CF00`. HIGH's top 4K is RAM under BASIC
  ROM (banked out at startup).
- **The RAM under the ROMs** (`src/lib/vault.s`). `$D000-$DFFF`, under the I/O
  area, holds the home continent for the whole game, 64 by 64 bytes of tiles;
  `$E000-$E7BF`, under the KERNAL, holds a continent put away while a town or
  a dungeon is in `local[]` (1984 bytes; getting in and out of all-RAM is
  `vault_bank.s`, apart so that a program can have it without the copies); the rest under the KERNAL is the
  music's, a tune's header at `$E7C0` and the tune at `$E800-$FFF9` (see
  Music). Reaching the vault takes the machine to all-RAM (`$01 = $34`), which
  takes the KERNAL and the I/O area away, so interrupts are off for as long as
  it lasts and the only code that may run is in ordinary RAM and calls nothing
  of the KERNAL's. An NMI cannot be turned off and its vector is read from RAM
  under the KERNAL too, so `vault_enter` (and the music, which plays with the
  KERNAL out) points it at `vault_nmi`, which banks the KERNAL in, runs its
  handler and banks it out again: the modem's RS-232 is NMI-driven, and a
  byte arriving while a tune plays would otherwise stop it for good (tested:
  the online dialog hangs with a bare RTI there and a slow tune). Everything is a
  copy of a screenful or a row (`vault_row`, `vault_tile`, `vault_park`,
  `vault_unpark`, about 30 ms at the worst) except building home, which the
  generator does in place, a few seconds with nothing else running.
- `local[]`, the town, dungeon or continent map, **is the font image** (2K of
  HIDATA), reused once `charset_install()` has copied it to `$3800`. Install
  the charset before anything writes a map; `charset_install()` refuses a
  second copy, since by then it would copy the map. Its last row (64 bytes) is
  also the row buffer home is drawn through (`world_view()` copies a row out of
  the vault into it), so a continent is 64x31, not 64x32, and nothing else may
  keep anything in that row while the overworld is drawn. (The ship draws home
  while a continent is still being built into the rest of local[].)
- `$0340-$03FF` **hero sprite** data (blocks 13 to 15, in the cassette
  buffer). Sprites must sit in VIC bank 0 like the charset; all three are in
  use. Nothing else may use the tape buffer.
- More sprite blocks come out of the **charset's reverse half**; see the
  glyph list above before taking one.

The PRG is one contiguous load, so MAIN and the charset gap are `fill = yes`.
That is why the small tools use `cfg/c64-tool.cfg` instead: without it a
1.4K hello world becomes a 14K one.

## Things that will bite you

**cl65 makes `foo.s` from `foo.c` in the source directory on its way to
`foo.o`, and deletes it afterwards.** An assembly module whose name matches a C
module's is overwritten and then deleted, with no complaint. The continent
land generator is `isle_gen.s` (next to `isle.c`) for that reason, and the
towns' is `tgen.s` (next to `town_gen.c`). The same goes for one `.c` compiled
twice: the open world's parts (`src/ow/*.c`, once for each engine) raced on
`src/ow/NAME.s` under `make -j`, and an engine linked the other's code or
did not assemble, with nothing said (it hung at start with the stack
pointer past its top). Those are now `cc65` and `ca65` through
`build/ow/NAME-cpu.s` and `-reu.s` (`OWPART` in the Makefile).

**Screen codes are not ASCII and not PETSCII.** They index the active charset.
q4k runs the lowercase/mixed set (`VIC.addr = 0x17`, i.e. `$D018 = $17`) because
cc65's `conio` emits codes for that set — leave it in the boot-default
uppercase set and every letter `cputs()` writes renders as line-art. In the
lowercase set `$41-$5A` are `A-Z`, `$01-$1A` are `a-z`, `$20-$3F` are ASCII 1:1,
`$A0` is a solid block. `make shot PROG=charset` shows all 256.

**VICE halts the CPU whenever a binary-monitor command arrives**, including on
the initial connect. Every read must be followed by `MON_CMD_EXIT` (`0xAA`) or
the emulated machine never advances. `tools/screendump.py` handles this; if you
write another monitor client, remember it.

**cc65 re-encodes character literals to PETSCII at compile time.** `'a'`
assembles as `$41`, `'A'` as `$C1`, `"PING"` as `$D0 $C9 $CE $C7`, and `'\r'`
as `$0A`. This is correct for `conio`, which expects PETSCII — and wrong for
every byte that touches the wire, the screen directly, or data from the host.
It has caused three separate bugs so far: garbled text output, a protocol that
sent shifted PETSCII, and range tests in `ascii_to_screen()` that compared
ASCII input against PETSCII bounds (uppercase fell through the lowercase
branch and came out as `$E1-$FA`). **Rule: any comparison or literal involving
non-PETSCII data uses numeric constants.** `modem_put_literal()` exists to
convert literals deliberately; `modem_puts()` sends ASCII buffers untouched.

**`$CF00-$CFFF` is reserved** for the probe page by `cfg/c64-q4k.cfg`. Don't
place game data there (bar BSS2, above).

**Colour RAM is write-only-ish nybbles at `$D800`** and is *not* covered by the
VIC bank switch — it stays at `$D800` regardless of where the screen matrix is.

**The open world's game lives in a block every one of its programs leaves
alone**: `OwGame` (`include/q4k/owgame.h`) at `$CE3F-$CEFF`, between the
engines' BSS (cut to end there) and the probe page: the party, the purse,
the quest, the looted bits and where everyone is (the area, the town or
dungeon and its level, the square, the world square, the ships, the boat,
the music, the effects and whether the party's lines are hidden, and the
dungeon monster last fled from with how many of it were left: `fled`). ld65 does not clear it. The engines' cfg names its party `party`
(it comes first), and they copy the rest in (`take_block()`) and out
(`owg_where()`, before a save or another program); a program that links
the game's own `actor.c`/`game.c` uses `owg_in()`/`owg_out()`
(`src/lib/owgame.c`). A save is this block as it is, in `ow-save` (193
bytes, `OWG_SIZE`: the whole of `$CE3F-$CEFF`, so a new field must move the
bound first -- `OWG_ADDR`, the engines' cfg (`_party` and BSS), `foes.s`'s
`FOE_A`, `stlow.s`'s offsets (engine.c checks them), `mkowsave.py` -- and the engines' BSS has 23
bytes below it; it is what `tools/mkowsave.py` writes too and the engine
checks): F1 in an engine writes it, after Y to "Save over the saved game? (scratching the old file first, as
`savegame.c` does; the border is light blue while the drive works, red
for a second if it would not, and the text window says which), and
F3 (after Y to its own question) goes back through the bootstrap with `OWB_LOAD`, which loads it and
builds its world (the game goes on as it was if there is no save). The
`openworld-save` case in `tools/test.sh` saves, walks on and restores, on a
copy of the disk. Every engine start resumes from it: the overworld, a town (at
its gate when `x` is `OWG_GATE`) or a dungeon level, where it was left.

**The figures** -- the party as the open world shows them -- are the
fight's: `assets/monsters.txt`'s warrior, archer and wizard, two frames
each, the second the first sunk a row onto its feet (`tools/figures.py`'s
`bob()`, which `mkfields.py` and `mkoverworld.py` share, and a villager
marked `bob` in `overworld.txt` gets too). Everyone idles between their
two every sixteenth frame, as in a fight: the hero (the leader's class,
`hero_frame()` in `engine.c`: frame `cls * 2 + stride`; a step still
changes it twice, so walking reads as walking), the townsfolk (`t_idle()`,
the town's `ov_idle`, which the core now calls in every area) and the
figure on the party screen. A text dump marks a multicolour sprite by the
middle of the 16 by 16 it is drawn in (doubled if it is), not of its
pixels, which the bob moves (`sprite_cells()` in `screendump.py`, as for
the fight's `--mux`); `--poke still=1` stops the engine's bob too, but only
until the engine is loaded again, after a talk, say. The engine's
sprite blocks from `$5000`: the three classes (0-5), the villager (6, 7),
the ship (8, 9). Each member's colour is `tunic[]` in the game block, from
the name at first (`ow_party.c`'s `tunic()`, sprite.c's `tunic_of()`
again; `mkowsave.py` has it too), and the hero is drawn in the leader's
(`sm_tunic`), a fight in everyone's (`ow_fight.c`). **The party screen**
(`I`, `party_look()` in `owtown.c`) shows one member at a time, with
`shop.c`'s rows (`party_head()`, `member_rows()`, `party_totals()`,
`party_prepare()`, which the game's own `party_draw()` is made of) and
their figure doubled below them: cursor left and right go through the
party, up and down through the colours (all but black), and `S` prepares
a wizard's spells. The figure is `src/lib/ow_figs.s` (generated, segment
`OWFIGS`, aligned to a sprite block in MAIN, which it asserts is where the
VIC sees RAM); room for it came from moving `tiles_install()`
(`TITLECODE`/`TITLEDATA`) and the party screen's code to HIGH, which only
a fight overwrites and neither runs after one. Golden `openworld-colour`
is the screen, and `openworld-recolour` in `tools/test.sh` changes
Alaric's colour and checks the engine's hero has it. The game proper is
untouched: its hero is still the three-sprite hi-res figure and its
townsfolk glyphs.

**The story in the open world** is the game's: `owtown`'s conversations
hand over what `gift.c`'s `quest_after_talk()` says (Garrett's arms, the
Sigil, the keys, Dragonbane, the reward), with the quest stage and the
items in the game block. Its gates say no with a sound (`SFX_NO`,
`sfx.s`, once a press: `bumped` in `engine.c`) and why, in the text window
(`refuse()`): Millford's gate before Garrett has armed the party
(`t_can_step()` in `town.c`), a dungeon without its key (`to_dungeon_if()`
in `world.c`), and a ship at one of home's ports before the story reaches
its continent (`ship_held()`: `Q_EMBER` + its slot, as the game's captains
have it; one moved, or at a continent, always sails). Golden
`openworld-garrett` asks Garrett for arms with the story at `Q_START` and
walks out. The quick start and the disk's save are at `Q_HEART` with every
key and stone, so that nothing is shut for the other goldens.

**Fights in the open world** are the multicolour arena's module (below),
called by `owtown` (`OWB_FIGHT`, `src/lib/ow_fight.c`). The engine rolls
after a step on foot as the game's `encounter_roll()` does (`wander()` in
`engine.c`, out of 200 by the ground, never in towns or ships, dice seeded
from the game's turn and seed; not in a dungeon, whose monsters are on
the map: below), sends the keeper (walked into: `d_bump()`) and
Maldrath's lair (with Dragonbane, before `Q_VICTORY`) the same way, and
leaves (`ow_fight_with()`). `ow_fight()` picks what turns up as
`battle_wander()` does, copies it out of the bestiary, loads the module
over everything of `owtown`'s above `$4000` (so whatever runs after the
fight, and the C library, must be in MAIN: `cfg/c64-owtown.cfg`), and then
pays, levels, hands over a keeper's stone, or wakes the party in Millford
with half the purse, and says so on a screen of text. The fight plays the
`fight` or `boss` scene's song (`fight_music()` in `ow_fight.c` loads its
file if another is under the KERNAL) and carries on with the engine's if it
is the same one, from its own interrupt (below), and `M`/SHIFT+`M` there change
the game block's `quiet`. Not yet: gear and treasure maps dropped, and
pay for the foes killed before a flight.
`OWG_CALM` in
the game block's `quiet` keeps anything from wandering out (the quick start
and the disk's save have it, so the goldens meet nothing); `--poke
enc_always=1` makes every roll a fight, which `openworld-fight` in
`tools/test.sh` uses to win against a goblin (and places a dungeon's
monsters in spite of `OWG_CALM`; `enc_always=2` does only that). The fields are packed on
the disk (`tools/mkfields.py`'s `pack()`, unpacked by the module from
`$C000`). **The open world's disk is a D81** (a 1581's, 3160 blocks): a
1541's was full once the fights were on it. `screendump.py` and
`tools/run.sh` put a 1581 in unit 8 for a `.d81`. A machine with no
splash picture is asked F1 or F3 in words (`title_text()` in
`src/openworld.c`).

**The text window, and `L`.** The engine's map is rows 0-21 of its screens;
rows 22 and 23 are two lines of text that stay put while the map scrolls
(`sm_say()`, `sm_ink` -- 0-7 only: 8-15 is a multicolour character, which
is how the captain's refusal in light grey came out as smeared yellow --
in `smooth.s`, which explains the raster split: at
line `$DC` the fine scroll is forced to 7 and extended colour turned on,
which with multicolour is black, so the text rows always start at `$E7`
behind a seven-line black band; a second interrupt turns it off in the
right border of line `$E6`). Its letters are the character ROM's
lowercase set, copied to codes `$A0-$FF` of the engine's character set, so
no tile set may have more than 160 characters (`mkoverworld.py` checks).
Anything that keeps interrupts off for long calls `sm_guard()` first (the
vault copies in `sm_vcopy` and `ow_gen.s`, the REU's transfers), or the
split comes too late to work and the window shows rubbish for a frame;
`--peek sm_splitline` is the line just after the last split's write
(`$DD`; it must have been written by cycle 58 of `$DF`). People's sprites below the
map's last row are not shown. `screendump.py --vicshot PNG` saves VICE's
own picture of the frame, which is the only way to see the split: `--shot`
draws from memory. `L` (`ov_look`, a fourth jump in each overlay's table)
is the game's `look()`: a town's hidden gold (`loot_table`), a dungeon's
chests (`dun_cx`/`dun_cy`, even odds gold or a draught), and elsewhere
nothing, or one time in `grave_odds` the dead (`OWF_UNDEAD`, the game's
`battle_undead()`); never in a town or with `OWG_CALM`. No treasure maps
yet. RUN/STOP asks to quit (Y or N, in the window) and goes back through
the bootstrap with the world block empty, which shows the splash again.
`?` is `owtown`'s `OWB_HELP`, the open world's keys and the quest's next
step.

**The party's lines, the full screen, and the engine's code below $0801.**
The text window shows the party as a fight's panel does (name; health, a
wizard's magic; the draughts and arrows in the last column with room), all
of it `src/ow/stlow.s`: `sm_say()` is its, which clears the lines off
before `smooth.s`'s `sm_say_raw()` says anything, and the next step
(`st_step()`) puts them back. **SPACE hides the window and gives its rows
to the map**: `_sm_rows` (smooth.s's DATA) is 22 with the window and 25
without, and the split (`irq`), the shift copy, the edges, the draws, the
REU's lengths and the people's lowest line (`_sm_npclim`) all read it. The
unrolled copies are the window's 22 rows: the colour copy goes on into
`colour_tail_a/b` (HICODE, a column of the last three rows a pass), and the
CPU's shift copy is 25 pairs, the 23rd patched to a jump past the last
three with the window (`patch`, made again when `_sm_scdir` is $FF). Hiding
it redraws the front screen there and then (`window_off`); `OWG_HIDE` in the
game block keeps it across programs (`st_start()` hides it again).
Anything said while it is hidden brings the window back, to stay
(`window_on`). Measured over a 40-step walk (`late`): PAL 0 either way; NTSC
10 with the window, 19 full screen (the full screen before the window
existed: 15); the REU's engine 0 either way. Golden `openworld-full`.
A new game opens with "Hail, adventurers! Press ? for help." instead
(`st_start()`: `game_turn` is 0; `mkowsave.py`'s save has a turn). F1 and
F3 ask first, in the game's words (`st_save()`, `st_load()`, the engine's
`yes()`). A chest, a dungeon's or the castle's (its two are `loot.txt`'s),
is opened by standing on it (`st_arrived()` after `ov_arrived()`, which
calls L), and any taken chest is drawn open, `chestopen` (`$95` in the
dungeon's and the castle's sets), in `WORLD` only: `st_chests()` from
`dungeon.c`'s `muster()`, `st_tchests()` at the end of `town.c`'s
`put_roofs()` (the castle's are in the throne room, and a roof taken off
puts back what `local[]` has), and after L the square redrawn as a roof
change is. The keeper
idles: `st_bob()`, with the hero's idle, swaps his four characters with
`keeperbob`'s (`$94`) in the character set; his art (`places_common.txt`)
is outlined in grey, since red and brown on a dungeon floor is what the
blood is. stlow.s, `sm_text_clear` and `font_copy` live in `OWLO`,
`$0430-$07F7`: **the engine's file starts at `$042E`**, in the KERNAL's text
screen, which the engine never shows. While anything loads, the disk icon
(`ow_disk.s`) puts the VIC in extended colour and multicolour at once,
which it draws as black with the sprites over it, so the code loading into
the screen is never seen; `disk_off` puts the KERNAL's mode back. Sprite 0's
pointer at `$07F8` is written with the icon's block (`OWSPTR`), since the
icon is up while the file loads over it. So `load_failed()` writes the
screen's top row only, and anything that hands the screen back to the
KERNAL first clears it (`ow_leave()`; `rebuild()` goes through it now) or
it shows code. `said`, the engine's line being made up, is the BASIC input
buffer at `$0200`. Goldens: the window's rows are the party's lines, or
what was said last.

**The music carries on** across the programs: leaving, the engine pauses
the tune where it is (`ow_tune_pause()`, the volume only) and says which
song in `OWB->song`; `owtown` plays it on from the KERNAL's interrupt
(`src/lib/ow_play.s`, which the talk's `music_pause()` pauses round the
disk), pauses it again before `ow_back()`, and the engine's `play_for()`
carries on with it (`ow_tune_resume()`) if its scene plays that song. The
same goes for a town's gate, an overlay's load and a save inside the
engine. A fight carries it on too: the module plays it (`ow_tune.s`) and
leaves it paused for the screen after and the engine.

**The screens of text are a program loaded from disk, not a part of the
engine**: `owtown` (`src/owtown.c`, linked with `cfg/c64-owtown.cfg`, the
flat layout with the shop's code below the charset, where it would be 6K
of padding in the file; the talk's code, which the inns' rumours use, is
above it, in HIGH, since no inn follows a fight, and MAIN wanted the room
for the REU cache's loader). The hand-off block (`OWB` in `include/q4k/ow.h`,
at `$02ED`) says what for: `OWB_SHOP` and
`OWB_INN` the game's own `shop.c` and `inn.c` (walking into a keeper, as a
townsperson), `OWB_PARTY` the party screen (`I`: `shop.c`'s rows, one
member at a time with their figure, below), `OWB_HELP` the keys (`?`). **A conversation (`OWB_TALK`) is a program
of its own**, `owtalk` (`src/owtalk.c`, `cfg/c64-owtalk.cfg`), which the
town's overlay loads for a person (`talk_to()`: a keeper or an inn goes to
`owtown`): `talk.c` and `gift.c`, with all its code below `$3800`
(3.5K free there, less the C stack), the font image, `tiles_install()`
and `actor.c`'s and `town_data.c`'s tables at `$4000-$4DC6`, and BSS at
`$B000` as `owtown`'s, so that VIC bank 1 is free for a portrait of the
speaker (a `.v2i`, once the library can show one in bank 1); before one
goes there, the tables the talk reads move below `$4000`. Both programs
link `src/ow/owstand.o`, the few variables and functions of `game.c`,
`sprite.c`, `savegame.c`, `music.s` and `view.c` that those modules use
(as the engine defines its own), ahead of the library, since `game.o`
has the same names; so
that none of those is linked, nor the map, the land generator and the
vault behind them: adding a module that references one of them can pull
8K back in, so read `build/owtown.map`'s module list after. `rand()`
starts from the same seed in every program, so `owtown` seeds it from the
game's turn (the engine counts its steps into `game_turn`) and seed, or
every visit would hear the same rumour. The rumours are the inns' own
files, which the offline talk already reads whole (a few hundred bytes
each): a REL file would let one rumour be read without the rest, which
nothing here needs. Golden `openworld-ale` buys a mug at Millford's inn. **The party's
making is a program of its own**, `owparty` (`src/owparty.c`, 7K, with
`cfg/c64-tool.cfg`: it all fits below the charset), which the bootstrap
loads after F1 (`OWB_NEW`) or the tests' quick start (`OWB_QUICK`): the
title screen's story and questions (`src/lib/ow_party.c`), the new game
into the block, and the engine over it at Millford. The engine
leaves with `ow_leave()` (the block out, the machine as `sm_quit()` leaves
it, `ow_run()`); `owtown` comes back with `ow_back()` (`src/lib/ow_back.c`:
reloads `ow-data`, which it overwrites, resets the machine and loads the
engine), and the engine loads the area's overlay straight away, with the
hero where they stood, and builds the overworld afresh only on the way out
(`fresh` in `world.c`, even with an REU, whose copy did not survive). The
talk's files are on `openworld.d81` beside it (a D81, with about 2300
blocks free). The RUN/STOP that leaves
a talk is still flagged when `ow-data` reloads, and the KERNAL's `LOAD`
then fails with no drive error: `ow_back()` waits for STOP to come up and
sets `$91` to `$FF` (released). `$7F` in `$91` means STOP is held down, so
clearing the bit with `& 0x7F` is the wrong fix. That was not enough (a
second press during the load broke it again, over and over), so from
`ow_back()` on, and in every program `ow_run()` loads, the KERNAL's STOP
vector (`$0328`) points at a routine that says it is never pressed
(`nostop` in `ow_run.s`'s loader, in the tape buffer): no load can be
broken. RUN/STOP still comes through `GETIN` as `$03`, which is how the
shops and the engine read it. The loader retries a failed load instead
of flashing the border for good. Golden `openworld-talk`
talks to Old Tam in Millford, comes back and walks out; it leans on the
harness typing its keys a bufferful at a time (the talk and the engine
each empty the buffer as they start), hence the `j` padding (`x` is a step now). **Never
`--poke` an engine's label in a test that starts in `owtown`**: pokes land
when the first stage is published, which is `owtown`'s, over its code.

**The open world's engine is a core and three overlays** (`src/ow/engine.h`).
The core (`engine.c`, `smooth.s`, and the library: the main loop, the
scrolling, the music, what every area does)
stays put; the world's overlay (`world.c`, `OVWCODE`: the overworld's window
and streaming, the continent kept, the ships, and the land generator with
the thread that runs it: `build/ow/ow_isle.o`, `isle_gen.s` assembled with
`OW_WORLD`, and `src/ow/genbuild.s`), the town's (`town.c`,
`OVTCODE`: towns and the castle, the roofs, the people, the talk's trigger,
and the town generator and the towns' maps: `ow_tgen.o`, `tgen.s` with
`OW_TOWN`, and `ow_town_gen.o`, `ow_town_data.o`, compiled into the overlay
through `build/ow/`)
and the dungeon's (`dungeon.c`, `OVDCODE`: the levels and their stairs, with
its own copy of the game's `dungeon_build()`, `build/ow/ow_dungeon.o`,
linked ahead of the library so the library's `MAPCODE` copy is not, as
the other generators' copies are) take
turns in `SLOT` (`$5A00-$7FFF`, 9.5K, to the end of the VIC's bank), each a file of its own that ld65 writes
beside the engine (`ow-cpu.world`, `.town`, `.dungeon`, and `ow-reu.*`,
since each engine links its own). Stepping onto a town or a dungeon loads
its overlay over the world's, and its way out loads the world's back
(`ov_switch()` in `engine.c`: the music stopped, the sprites off, the screen
already blank; if a load fails, `load_failed()` shows the file and the
drive's error message on the KERNAL's screen, red border, and tries again
after a key; the same for the engine's `.hi` file at the start); a dungeon's stairs
between levels load nothing. Their BSS stays where it is, so the world's
window queue, the continent kept and so on are as they were left, and the
REU's copy of the window is put back as before. The core calls an overlay
only through the jump table at the start of the slot (`ovl.s`: `ov_enter`,
`ov_can_step`, `ov_arrived`, `ov_bump`, `ov_after`, `ov_idle`, `ov_look`,
and `ov_gen`, where a continent's build starts: the others' never run), and an
overlay asks for another by setting `ov_want` and returning, never by
loading it itself (it would load over its own code). `tools/owslots.py` runs
after each link and fails it if anything else calls into an overlay's code.
`owslots.py` knows the generators' copies by name (`part_of()`), and
`isle_rnd`, which both use, stays in the core (`MAPCODE`, 25 bytes).
One link makes them all, so `ow-data` (COMMON) is still one file for both
engines. A new overlay is a file in `OWPARTS`, a code segment and a memory
area in the cfg, a table in `ovl.s` and a line in `owslots.py`'s `SEGS`.
Goldens `openworld-dungeon-out` and `openworld-out` go in and out of a
dungeon and a town.

**A dungeon's monsters are on the map** (`src/ow/dungeon.c`, and
`src/ow/foes.s` for what is done square by square, which in C did not fit
the slot): six a level (`OWG_FOES`), placed the first time a level is seen
after the way in from the world (`foe_made`, a bit a level, cleared by
`d_enter()`) on open floor at least five squares from either stairs, as
`battle_wander()` would choose them (by the party's level and the depth,
a quarter natives over the sea), and kept in the game block (`foe_a`,
`foe_b`: two bytes each, x and y and a five-bit kind; x 0 for one there
was no room for, and kind `OWG_FOE_DEAD` for one killed, whose square
gets a bloodstain: `foes.s`'s `stain`, the dungeon set's `blood` at
`0x92`, written into `WORLD` only, so it is still floor underfoot),
so that a level left by the stairs and come back to has the same ones
where they were left, and a fight's round trip through `owtown` too. Each
is a sprite, 1 to 6, through the town's people's `sm_npc_*` (with
`sm_npc_ox/oy`, pixels off the square, for the glide), one picture a kind
(`tools/mkfields.py` writes `src/ow/foe_art.s` from `assets/monsters.txt`,
`OVDART`, aligned to a block after the overlay's code, and asserted below
`$8000`, where the VIC's bank ends: only one frame was given room), however many
the fight brings. After each step of the hero's, each that sees them (no
farther than the screen, 10 by 6, and nothing but floor on the line)
comes for them at its speed in the arena (`Monster.speed`, tenths of a
square a turn, banked), one monster a frame in `ov_idle` while the raster
is above line 190 (`foe_room()`), gliding there at the hero's pace; one
beside the hero after its turn, or walked into, is a fight (`OWF_FOE`,
which `ow_fight()` fights as that kind and clears from the block if won;
one fled from stays). The fight waits for its glide to end, and the hero
cannot step while one is coming (`pending`, `d_can_step()`); a held key
starts the next step before the turns, so one that reached the square
the hero was leaving takes another step after them (`d_arrived()`).
A monster's square goes back into the block with its kind's bits kept:
square + `BIAS` has bits of its own there (a bone archer once came back
a wasp). `foes.s` has a copy of what the map needs of `bestiary.c`
(picture, colour, speed: in RODATA, which is COMMON, for the slot's
room) and `dungeon.c` of the wanderers'
`from`: change a monster there and here. `OWG_CALM` places none, unless
`--poke enc_always=2`.

**The open world's disk icon is resident at `$CF14`.** `src/lib/ow_disk.s`
(segment `OWDISK`, `cfg/c64-owboot.cfg`) holds the code, the sprite's picture
and the interrupt, and the loader stub (`ow_run.s`) calls it at fixed
addresses `$CF14` (show) and `$CF17` (stop) before and after every `LOAD`. The
bootstrap copies the block there, and it must do so after the splash, not at
the start of `main()`: the splash unpacks its picture into `$C000-$FFFF`,
which covers `$CF14`. The sprite lives at `$0380`, the only place in bank 0
no program's code is. The interrupt stops itself after about thirty seconds,
so a soft reset during a load cannot leave it blinking at READY. The talk
program's BSS2 starts at `$CF14` too, but nothing there writes it.

**The REU load cache** (`src/lib/ow_cache.s`, `ow_cload()` in
`include/q4k/ow.h`): with an REU, a file the open world loads comes from the
disk the first time, is stashed in the REU at once (before anything runs in
it or writes to it), and is fetched by DMA every time after: `owtown` in
about 33 ms. Every load but `OW-SAVE`'s and the splash pictures' goes
through it: `ow_run()` (the engines, `owtown`, `owparty`), `ow_back()`'s
`ow-data`, the engine's `load3()` (the overlays, the `.hi` files, the tile
sets, a tune), `ow_fight.c` (the module, the tunes, and the field and the
sprites, which it loads itself before calling the module and says so with
an empty sprite name: the module links no cache, or its BSS would reach
`owtown`'s at `$B000`), and the bootstrap's `ow-data` and tune. Where things
are is the build's decision: `tools/mkowcache.py` knows every cacheable
file's name, address and length (`OWCACHE_FILES` in the Makefile, the most
used first), and writes the bootstrap's table (`build/ow/owcdir.s`) and the
build's stamp, a hash of them all. In the REU, bank 1 (bank 0 is the
scrolling's): a directory of 256 entries of 8 bytes at 8 times a hash of the
name (the file's room, its length, its address, and a check byte from the
name, bit 0 set once it is there), the magic and the stamp at `$0800`, the
rooms from `$0900`. The hash's salt is `include/q4k/owcache.inc`'s, and
`mkowcache.py` fails the build if two names share an entry (`mkowcache.py
salt NAMES` lists the salts that work: 1 is chosen with the bootstrap,
the splash pictures and `ow-save` counted in too). The bootstrap lays the
directory out (`ow_cinit.s`) unless the REU already has this build's stamp
(N and F3, and a preloaded image, keep it): the REU's size from a byte
written to the top of each bank, highest first, read back up until a bank
does not hold its own number; the directory cleared (a stash with the
C64's address fixed); each file an entry and a room while they fit; the
magic last. A load looks only at the magic (with no REU nothing comes back,
so it is never there) and the entry's check; a name the build does not know
loads as it always did. **A miss is one load and one stash**, then a stash
of the entry's last byte: the KERNAL forgets a file's own start address
during a LOAD (`$AE/$AF` are overwritten from the file's first two bytes),
which is why the start comes from the table. A stash runs with `$01 = $35`
(a tune at `$E7C0` is under the KERNAL) and the NMI's vector, read from RAM
then, on an RTI; every transfer has interrupts off from its setting up to
its end, since the engine's raster interrupt uses the REU too. The cache's
scratch is segment `OWCBSS` (in every cfg; in an engine past `OW-DATA`'s
last byte, since BSS is full). `ow_run.s` looks a program up in `_ow_run`,
while the caller is still there, and its stub only fetches (`cpre`) or loads
and stashes and marks (`cpost`); that half, with the name, is copied to
`$0100-$017F`, the bottom of the stack page, since the tape buffer was full.
`cache_hits`/`cache_misses` count an engine's (and every program's) for
`--peek`. Tested: every `-reu` golden; the tune stashed from under the
KERNAL fetched back byte for byte; and `openworld-cache` in
`tools/test.sh` (below).

## Treasure maps

A fight won on the overworld drops a map one time in `treasure_odds` (10;
`--poke treasure_odds=1` makes it every time). It picks a random plain
(`TF_PASS`) square of the land the hero is on, and the map says how far east
or west and north or south of a place it is, in leagues, a league being a
square: the nearest one on that land (`place_near.s`; `anchor_name()` in
`loot.c` names it). `tmap[]` holds four of them, six rows of four bytes
(land + 1, x, y, the anchor's x, y, and which of the land's places it is;
land 0 is an empty slot) and is saved with the game (`Q4K7`). `I` lists them
(`treasure_maps()`), and `L` on the square (`dig()` in `loot.c`) digs it up and
tears the map. Dungeon fights drop nothing: only home is readable from the
vault wherever you stand, and a continent's tiles are not in `local[]`.
Nothing is stored for a map beyond those bytes, and nothing needs `looted[]`.

## Music

**SID files, one scene at a time.** Every tune is a standard SID file (PSID):
a 6502 player and its data, with an init routine (song number in A) and a
play routine called 60 or 50 times a second. `assets/music.txt` maps each
scene (`home`, `isle`, `town`, `castle`, `dungeon`, `sea`, `fight`, `boss`,
`talk`, `inn`, `shop`) to a SID file and one of its songs; `tools/mksid.py
map` makes that into `include/q4k/music_map.h` (`MUSIC_HOME` ...) and
`src/lib/music_map.c`. The code says which scene it is in with
`music_scene()` (`src/lib/tune.c`, in the core): the map loop each turn,
from where the party stands (`map_music()` in `q4k.c`), and the fight
(`fightcall.c`), a conversation, an inn, a shop (`q4k.c`) and a voyage
(`sail.c`) before they start. A scene whose file is not the one in memory
loads it from the disk (`m.NAME`), with the music paused and the sprites
hidden, and starts its song; the same file with another song only starts
that song; the same file and song carries on, which is why the theme now
plays straight through a fight. A file that will not load leaves the music
off without asking for the disk (the music is not worth stopping for), until
a scene wants another. The help and party screens keep whatever is playing.

**Where it lives.** One tune at a time, in the RAM under the KERNAL: the disk
file loads at `$E7C0`, a 64-byte header that `mksid.py` writes (init, play,
song count, the volume to restore after a pause, a byte a song for its
speed), then the tune at its own address, which must be inside
`$E800-$FFF9` (6K). `src/lib/music.s` reads the header into its own code
(`music_play()`), and calls play from cc65's interruptor chain with the
KERNAL banked out (`$01 = $35`) and back in after; see the RAM under the
ROMs, under Memory, for the NMI that this needs. **The KERNAL's interrupt is
60 Hz on PAL as well as NTSC** (its CIA timer is `$4025` on PAL, `$4295` on
NTSC; measured: 151 jiffies in 125 PAL frames): a song made for the frame
rate (PSID speed "VBI") is called five times in six on PAL, 50 Hz, and a
"CIA" one every time. (The old built-in player took PAL for 50 Hz and played
the theme about 18% fast there.)

**What `mksid.py disk` checks** before a tune goes on the disk, by running
init and 2000 calls of play for every song, on both clocks, in a small
emulated 6502 (`tools/sid6502.py`, official opcodes only): that it is a PSID
(not an RSID) with a play routine, for one SID, loaded inside the area; that
it runs only there, writes only there, voices 1 and 2, the filter and volume
(`$D415-$D418`) and zero page `$FB-$FE`, and reads only those, the stack and
`$02A6`; that it uses at most 64 bytes of stack and `PLAY_BUDGET` (4000)
cycles a call. **Voice 3 is the sound effects'**, so a tune that touches it
is refused. A tune refused for its address needs relocating to `$E800`
(GoatTracker exports at any address; `sidreloc` moves an existing one).
The tunes the build makes are `build/sid/NAME.sid`, the rest go in
`assets/music/NAME.sid`; `make` also copies the theme to `dist/theme.sid`,
which plays in any SID player. A new SID file costs nothing in memory and a
few blocks on the disk (283 were free).

**The theme** is one of those SID files. `assets/theme.txt` is the score
(`~` slurs a note into the next, `v` adds vibrato); `tools/mkmusic.py` turns
it into `src/sid/theme_data.s`, which `src/sid/theme.s`, the player, plays;
`cfg/sid.cfg` links the two at `$E800` and `mksid.py wrap` makes the PSID
(470 bytes). Its speed is CIA (60 Hz on both machines) and it picks its
pitch table by `$02A6`. It plays a triangle lead on voice 1, and on voice 2
a quiet triangle (the pulse it started as was too loud and too bright)
holding one note a bar, a perfect fourth below whichever of the bar's melody
notes leaves the least clashing, resting for the bar's last eighth
(`HARMONY_REST = False` in the generator never re-strikes it: only the pitch
changes). A quick attack on a quiet SID voice clicks, because the envelope
always climbs to full level first; voice 2 uses a 100 ms attack for that
reason. (`HARMONY = "long"` in `tools/mkmusic.py` moves it only on
long notes instead, and `h` marks in the score force a move there). The
generator works the harmony out; the score is just the tune.

**Pausing.** `music_pause()`/`music_resume()` wrap the disk code, which the
interrupt would steal cycles from. Pausing stops the calls, zeroes the
master volume and leaves the gates alone, so the notes carry on where they
were; it also *waits twelve jiffies*: with the drive emulated, VICE makes no
new sound while the drive works and replays its last audio buffer, so a note
sounding at the moment the disk starts is held for as long as the disk takes
(a five-second drone on a save; the SID itself had been told to stop, as
`-sounddev dump` shows). Silence has to reach that buffer first. `M` stops
the tune (its voices let go) and starts its song again from the top.

**Listening and measuring.** `x64sc ... -sound -sounddev dump -soundarg
out.dump`, through `tools/screendump.py --vice=...` (its `+sound` comes
first, so `--vice=-sound` overrides it), writes every SID register write
with its cycle delta in warp mode: that is how the SID theme was checked
against the old player, note for note and frame for frame. (Without warp,
with `--rs232`, the dump writes register snapshots instead.) To hear it:
`-sound -soundrecdev wav -soundrecarg out.wav` with the game past its title
screen, `-warp` and `-silent` stripped (or `-sounddev wav -soundarg out.wav`
if VICE's coreaudio device is unavailable). `--peek music_state` says
whether it is playing; the tune's own variables are under the KERNAL.

Sound effects are on voice 3, from the same interrupt, and work whether or not
the music is on (with a tune playing they leave the volume register to it) (`M` mutes only the music, `X` only the effects; both are handled in the
map loop in `q4k.c` and in the fight loop in `battle.c`, and cost no turn):
`sfx_step(tile)` for a step, which picks a sound by terrain from a table
indexed by screen code and gives the two feet slightly different pitches, and
`sfx_play(SFX_MELEE / SFX_BOW)`. `sfx_delay()` waits until a quarter of a
second after the last effect began, so back-to-back hits in a fight do not
blend, and returns at once with the effects off. Each sound is a short table
of steps in `music.s` -- frames, waveform, pitch, pitch change per frame. Two things about the SID cost an afternoon.
Noise gets *quieter* as its pitch rises: a full-level burst at a high pitch
measured 17 dB under a pulse thump, so crackle wants low pitches. And the
envelope's attack can start up to 32 ms late when its rates change (the ADSR
delay bug), so an effect kept quiet by closing the gate early, before the
attack finishes, wandered by 12 dB from one play to the next. Keep the gate
up for the whole effect, at least three frames at a stretch, and set the
loudness with the sustain level. A pulse is far louder than noise or a
triangle at the same sustain. The terrain steps were balanced by recording
each and measuring it; do the same for a new one.

## The land

Home -- Keldmarch -- and the four continents across the sea come out of one
generator, `src/lib/isle_gen.s`, from `lore_seed` and a compass point (0-3 a
continent, 4 home): the same land every visit, a different one in the next
game, none of it stored anywhere. It has an LFSR of its own, so it touches
neither `rand()` nor the encounter seed and nothing needs reseeding after it.
The land is one random walk stamping pluses and 3x3s, so it is connected by
construction, laying down the biome the walker is currently "in". Mountain
ranges and lakes are walks that only turn a square into an obstacle when
`removable()` says the walkable edge neighbours stay joined round it: a purely
local test, sufficient (not necessary) to keep every walkable square
reachable.

Home is 64x64 and has more land (a longer walk) and two kinds of ground, plains
or forest; a continent is 64x31 in `local[]` and any of five. A *special*
square (a landing, a town, a castle, a dungeon, a lair) is an even `G_` value,
so as far as connectivity goes it is not there: stepping onto it does
something, so nobody walks *through* one. Placing one has to leave the rest
joined and itself reachable, which is what keeps that true: the landings go in
first, with the land beside them marked `PROT` so no obstacle takes it, and
the places go in last, each on a square `removable()` allows, at least
`LANDING_APART` from a landing and `apart` from the places before it (given up
two at a time every 128 tries, so a small land cannot stop it); a lair wants a
mountain beside it. Home has four landings, one on each coast (the first land
along the middle row or column), and five places: Millford, the castle, the
Hollow Crown, Maldrath's lair and a town. A continent has one landing and two
places: its story place (a town, Wardlow or Ashcombe, or a dungeon, the
Drowned Stair or Blackpit) and a town. `tools/isleref.py` is the same
algorithm in Python, draw for draw; `tools/checkisle.py` searches it for
unreachable squares over every seed and compass point, then has the emulated
machine (`src/isletest.c`) build the same land and compares a checksum of each,
and parts of two maps, with the model's. Change the generator in both places or
that check will say so; it runs in `make test`. It is assembly because in C it
came to 1.3K, when HIGH had 700 bytes.

Which continent holds which story place is a shuffle of `lore_seed`
(`slot_of()` in `game.c`; `homepath.py` has it too): slot 0 is Wardlow, 1 the
Drowned Stair, 2 Ashcombe, 3 Blackpit, which is the order the quest wants
them in. A port opens when `quest_stage >= Q_EMBER + slot` -- until then the
captain "will not sail yet" -- so the continents open one at a time and the
captains need no state of their own. Home's places are `home_fx/fy` (copied out
of `isle_fx/fy` when it is built, because building a continent overwrites
those) and a continent's are `isle_fx/fy`; `place_at()` says which place a
square is, and `q4k.c`'s `arrive()` turns that into a town or a dungeon.

Going in to a town or a dungeon from the overworld remembers where
(`ret_x/ret_y`) and, from a continent, puts it away in slot 0; coming out puts
it back. `land` says which one you are on (4 is home) and is saved.

Four ports, one on each of home's coasts. A ship (`src/lib/sail.c`) takes you
to the continent of its own: the voyage is a camera, not a ship. The ship
(`T_SHIP`, a screen code written over the view with the hero sprite hidden)
stays in the middle of the viewport and the land moves. Leaving, the home coast
slides off the far edge until the view is open sea; arriving, the new coast
slides on from that same view, so there is no seam, and the last frame is the
one the map draws next. `world_view()` is the one drawing routine for both: any
position on home or on a continent in local[], plus a window (`world_x0..y1`)
outside which it draws only sea. Positions are biased by 64 so that going a
little off a map stays a small positive `unsigned char`, which cc65 does
cheaply; signed arithmetic tripled the code. After the slide on, the camera
pans to where the map's own clamped camera would be, so a landing near the
edge still ends without a jump. The continent is built (`isle_work()`, a
couple of pieces a frame) while the old coast slides away; it is about two
seconds of 6502, and a ship left standing at sea for that long looked like a
hang. Home is the same in the other order, except that it is already built.
Stepping onto the port is the only trigger, so arriving on one does not sail
again.

Every land has a town of its own, made by `src/lib/tgen.s` (in C it came to
1.8K): an inn, a shop (a smithy, an armourer or a bowyer), a third building
(a potion shop, or an empty house small or large) and up to two houses with a
villager in, on a grid of six lots with a street between, each building
bottom-aligned in its lot with its door in the bottom wall, so nothing overlaps
and every door opens on a street by construction. The keeper stands just
inside the door and what the building sells is on the corners of its top row.
`tools/townref.py` is the model, and `checkisle.py` checks all of that and that
the C64 builds the same towns. The ground and the scenery in the lots follow the
land's biome. Home's biome is `home_biome`.

Home is built when the story first needs it (stepping out of Millford, or
loading a game past it) and the first time takes a few seconds with nothing
else running: see Memory.

## The smooth-scrolling prototype

The open world (`make run PROG=openworld`) is a standalone experiment, not
the game: a world from the game's generator, walked with a camera that
glides a pixel or two a frame over 16x16 multicolour metatiles (2x2
characters), for the TeensyROM+/REU ideas. The game links none of it.

**It is made for VICE and the TeensyROM, not a real 1541.** Loads are
cheap there, so loading another overlay or program from the disk is a fair
price for memory; how long a stock drive would take is not a reason against a
design here.

**A disk of its own** (`dist/openworld.d81`, a 1581's, which `make` builds): a
bootstrap, two engines and what the bootstrap loads for them. The first, `openworld` (`src/openworld.c`, `cfg/c64-owboot.cfg`),
is a bootstrap: it looks for an REU (`ow_reu_detect()`, `src/lib/ow_boot.s`:
the registers, a round trip, and that a transfer lands in colour RAM, in its
last 24 bytes, which nothing shows -- the TeensyROM question), loads the
music a game starts with (a SID file of the game's, `m.*`, to `$E7C0`),
builds home into the vault and each continent once, for where its places
and its landing are, moors the ships, leaves all of it in
the hand-off block at `$02A7` (`include/q4k/ow.h`) and loads an engine over
itself (`ow_run()`, `src/lib/ow_run.s`, a loader copied into the tape
buffer; what only the bootstrap uses is in `ow_boot.s` so that the engines
do not carry it). The engines are
`src/ow/engine.c` and `src/ow/smooth.s` built twice: `ow-reu` (`-DOW_REU`,
`--asm-define OW_REU=1`) if the REU's transfers reach colour RAM, `ow-cpu`
otherwise, each with only its own way of scrolling. Both link the land
generator, to build a continent when it is wanted (below). `N` goes back through the bootstrap with the next
seed in the block (`sm_quit()` gives the machine back to the KERNAL first).
An engine has its own memory map (`cfg/c64-openworld.cfg`: VIC bank 1 at
`$4000` for two screens, the charset and the sprites, a 64x64 window of the
map at `$B000` and its edges at `$A000`) and its own raster interrupt, which
chains to `$EA31` with the CIA's timer interrupt off. **Tests** start the
bootstrap with the disk and read the engine's labels:
`screendump.py dist/openworld.prg --disk dist/openworld.d81 --splash quick
--lbl build/ow-cpu.lbl` (`ow-reu.lbl` with `--vice=-reu`); the bootstrap and
the loads take about fifteen seconds.

**The splash screen** comes first (`src/lib/ow_splash.s`, round the libraries
of `assets/splash-kit`, which is vic2orious's output: do not edit `lib/` or
`data/` there, regenerate them). The bootstrap finds the video system
(`ow_video()`), loads `splash-p` or `splash-n` at `$4000` (the kit's `.v2t`
in a page and then its `.v2i`, joined by `tools/mksplash.py`) and shows it
until F1, a new game (its seed is 1 plus the frames the player took: the
lands are built, then `owparty` makes the party and the game starts at
Millford's gate), or F3, a saved one: `ow-save`, the game block, which the
disk comes with one of from `tools/mkowsave.py` (seed 7, a lone warrior at
the castle). The tests' `--splash quick` (`OW_KEY_QUICK`, `$87`, no key on
the matrix) is the prototype's old start: a lone warrior, Alaric, with every
dungeon's key, on the overworld outside Millford, made without asking;
golden `openworld-party` is the F1 path.
**While the picture is up nothing else can happen**: the KERNAL is out, so
no disk; the picture is unpacked into `$C000-$FFFF`, so the lands are built
after it, and the bootstrap's BSS is at `$A000`; `$02`, half of cc65's stack
pointer, is the picture's and is saved round it. The module is assembled
`-t none`, because the libraries compare the files' magic with ASCII strings.
The keys are read from the matrix, which the tests cannot press: while it
waits `OW->hook` is `$A5`, and `screendump.py --splash f1|f3|quick` writes the key
there (then the new game's seed is 1, as the goldens have it). `N` skips all
of this. Golden `openworld-load` is the F3 path. The bootstrap says nothing:
the screen is black until the picture is up, and from the key until the
engine has the screen a yellow floppy blinks in the middle (`ow_disk_on()`
in `ow_run.s`: sprite 0 at `$0380` and a few instructions in front of the
KERNAL's interrupt at `$03C0`, in the tape buffer past the loader, since they
must outlast the bootstrap; it stands still while the lands are built, with
interrupts off). Only a failed load prints anything.

**What both engines read and neither changes** is one file, `ow-data`
(2.8K from `$8800`: the tables every tile set shares, in `OWDATA`, the
towns' maps and names, and every module's `RODATA`), which the bootstrap
loads once, after the splash, and which is still there after `N`. **The
tile sets are files of their own**, `t.overworld`, `t.town`, `t.castle`
and `t.dungeon` (`tools/mkoverworld.py --sets`, 1.6K to 2.1K each), which
the core's `tileset()` (`engine.c`, called by `build_area()` and the
world's `back_home()`) loads into SHORE (`$A000`, their own address) and
`sm_tileset()` unpacks: nothing reads a set after that, and every way into
an area builds SHORE afresh afterwards (`show_area()`'s `sm_shores()`, or
the REU's copy of the overworld's, which `back_home()` puts back after the
set). A set already in is not loaded again (a dungeon's next level; only
the overworld's set animates its characters, its water), and the music, if
it was going, goes on after the load as after a save. ld65 writes `ow-data` beside each engine (`COMMON` in the cfg,
`%O.data`) and the Makefile fails if the two differ, so only bytes that are
the same in both links can go there: data, not code (the engines' BSS
differs). `local[]` (`HIDATA`, the game's font image) is at `$8000` in an
engine and in no file, and COMMON is `$8800-$9FFF`. An engine is 13.1K (OW-CPU; OW-REU 12.6K), MAIN only: what it has past the
screens (`VICTOP` and `HIGH`, `$5280-$59FF`) is a file of its own,
`ow-cpu.hi`/`ow-reu.hi`, which `main()` loads before anything else, so
that the engine's file does not carry 5K of empty screens to the disk. The overlays are 5.9K (world), 6.4K
(town) and 4.5K (dungeon, its monsters' pictures at the end) of the slot's
9.5K (`make`'s `owslots:` lines). Free, OW-CPU's (OW-REU has a little more), measured from the maps: 1912
bytes in MAIN (the generators went to their overlays, and the REU cache's
loader came in; the engine no longer links cc65's `conio`: its one message is
written to the text screen's top row, and the keyboard's buffer is emptied
with a poke of `$C6`), 8 in OWLO (`$0430-$07F7`, below the BASIC stub: `stlow.s`), 19 in
OWTOP (`$5280-$53FF`), 105 in HIGH (`$5400-$59FF`, where the text window's
routines, stlow.s's helpers and the engine's words are), 3669 beside the
world's overlay (which has `wander()`, the overworld's encounter roll, out
of the core), 3210 beside the town's, 5120 beside the dungeon's, 18 in BSS, and 3.3K in COMMON (`$8800-$9FFF`, less the cache's 28 bytes of scratch). String
literals go to RODATA, which is COMMON, whatever `#pragma rodata-name`
says: what the engine says is `static const char[]` under the pragma. Which
part a function belongs in is which file it is in: the core's if more than
one kind of area uses it.

**The world** is a torus of 6 by 4 cells of 64x64 squares (`grid[]`):
Keldmarch in the middle of the second row, the four continents (64x31, at
rows 16-46 of their cells) north, east, south and west of it two cells
away, the rest open sea; it wraps both ways. Home is built once, by
the bootstrap (a few seconds), and kept in the vault at `$D000`. **One
continent is kept**, at `$E000-$E7BF`, because the rest under the KERNAL is
the music's, as in the game: the one nearest the hero (`kept`, `seek()`,
`nearest()` in `engine.c`). The window reaches 16 squares and no two
continents are within 65 of each other, so only one is ever in reach, and
when another becomes the nearest the one kept is at least 33 away. The
other is then built by the game's generator into `local[]` (free on the
overworld) and copied into the slot a row at a time, **as a second thread**
(`src/lib/ow_gen.s`, the core's half: starting, running and preempting it;
the thread itself is `src/ow/genbuild.s`, in the world's overlay, entered
at `ov_gen`): the main loop calls `gen_run(frame)` while it waits
for the next frame, and the frame's interrupt takes the machine back by
pushing one more frame for `$EA31`'s RTI, so the scrolling never waits. The
thread has the bottom half of the stack page and its own copy of the ten
zero-page bytes the generator uses. If the window needs a continent before
it is ready (those north and south are 17 squares off home's coasts),
`need_isle()` runs the build out there and then and the world stands still
for the rest of it, with the music playing; going into a place drops a
build, since a place's map is in `local[]` too. `gen_run()` checks
`sm_frame` with interrupts off before it starts: an interrupt between the
caller's look and the switch would otherwise give the build a whole frame
(that was 37 missed frames a voyage). And **nothing may be added to the
frame a step ends in**: `seek()` and the scene check went there first and
cost NTSC a missed frame every other step. Measured over the 59 steps of
`openworld-sail`: `late` 0 on PAL and with the REU, 4 on NTSC without (1
before any of this; 2 with VICE 3.7.1 before the generators went into the
world's overlay, 1 after). `wx`, `wy` are the hero's world
square; the engine's coordinates are those mod 128 (384 and 256 are both
multiples of it, so the wrap is seamless), and `WORLD` is a window indexed
`(Y & 63) * 64 + (X & 63)` (`sm_torus` makes `char_at` and the edges wrap
the same way). **Streaming:** each step queues jobs (`stream()`): the column
or row it brings within `LOAD` (16) squares, copied out of the vault by
`sm_vcopy` (or sea), and the edges of the column or row it brings within
`EDGE_X`/`EDGE_Y` (14, 10; the view needs 11 by 7), in thirds. `jobs()` runs
them in whatever is left of a frame, by the raster, and learns what each
kind costs (`job_lines[]`); a full queue (32) runs one on the spot. Leaving
for a place finishes the queue first. **Ships**: eight (`ship_x/ship_y`),
moored beside home's four ports and the continents' docks at the start.
Stepping onto one boards it (`boat` is its index + 1; the hero's sprite
becomes the ship, frames 8 and 9); in it you sail open water and step off
onto any land, where it stays; its position is kept as you sail. No
permission is asked. The continents' places go in as they do in the game
(the bootstrap works out `slot_of()` for each).
**The music** is the game's: the same SID files at the same place, and
the game's scenes (`assets/music.txt`: home, isle, sea, town, castle,
dungeon), through `play_for()` in `engine.c`. `src/lib/ow_tune.s` is the
player: `music.s` without the effects, called from the raster interrupt
after colour RAM is rewritten (so with the CPU scrolling a note can come 12
ms late on the frames the screen flips), every frame for a tune made for the
frame rate and six times in five frames on PAL for a 60 Hz one. `OW->tune`
says which file is under the KERNAL, across `N` too. A scene whose file is
another loads it in the engine, with the screen off and the sprites
disabled, the interrupt still running; nothing does that today, since every
scene plays the theme, but it was tried with a second file for the towns
(the town goldens passed, in and out). `M` turns the music off and on.
`--peek ow_tune_on`, `--peek '$02EB'` (the file) and `--vice=-sound
--vice=-sounddev --vice=dump --vice=-soundarg --vice=FILE` show it playing.

**Memory**: each engine has what is said above free: the REU's has more
because it has one colour copy, not two
(`colour_for` patches its hundred screen addresses for the screen a
redraw or a roof's flip reads, in the main loop before the flip, about 2K
cycles; every other flip is coloured from the REU).
Past the sprite blocks at `$5280` there is a little code (`OWTOP`).
`OWLOW`, code run at `$0400` from inside screen A's space, is empty since the
roofs went to the town's overlay, and nothing copies it down any more: ld65
leaves an empty segment's `__OWLOW_LOAD__` undefined, so put the copy back
with whatever goes there; the C stack is moved to the tape buffer
by `sm_stack()`; every engine load is tried three times (`load3()`), and
`load_failed()` shows the file, `E` and the KERNAL's error code (0: LOAD
said yes but returned an end address of `$0000`) and the drive's status; `ow_chcol` is page aligned (`ALIGNBSS`). The CPU's sixteen
shift routines are one, patched for each direction. **The quiet frame**: at
speed 2, the frame after a step's second flip builds nothing, so it shifts
the back screen for the next step if its direction can be seen coming (a
key held, or one waiting in the buffer: `pre_dir`), and the frame that ends
one step and starts the next does not have to (on NTSC it has no room for
it). A flip or a redraw cancels it. With the CPU scrolling, NTSC still
misses the odd frame when a step's direction could not be seen coming (the
test harness's keys arrive ten at a time, so it shows more of them than a
player would: 7 on the long voyage north); PAL and the REU do not.

`src/ow/smooth.s` explains the engine: the back screen is the front shifted a
character (`sm_shift`, about 10K cycles) plus the new row and column from the
map (`sm_edge_row`, about 6K, `sm_edge_col`, 4K), each in a frame of its own; on the frame the fine
scroll wraps, the interrupt at line 250 flips `$D018` and rewrites colour RAM
top down from the new screen (about 15K cycles), which keeps ahead of the
beam because each character has one colour (`ow_chcol`), so colour RAM is a
function of the screen. Measured (`--peek sm_colend`, the low byte of the
line the last row finished on): it is done at line 183 on PAL, about
60 lines before the beam needs it, and at line 228 on NTSC, about 12. NTSC is
the tight one: with the shift and the edge in the same frame, the frame after
a flip overran on NTSC (18 frames missed in 27 steps) and never on PAL. To
run on NTSC, `screendump.py --vice=-ntsc`. The camera
rests 4 pixels into a character so the first wrap of a step is two frames
away either way; steps are a square each, as in the game, so the direction
of every shift is known in advance. Eight ways (Q, E, Z, X or the stick's
diagonals; directions 4-7 after `sprite.h`'s four): a diagonal step is one
diagonal shift per flip, and on a mixed one (down-left, up-right), whose two
fine scrolls would wrap two frames apart, `glide()` holds the positive axis a
pixel back until the last frame so that they wrap together. A diagonal may
not squeeze between two corners of sea or mountain (`can_step()`). **Never build into the back screen in
the frame a flip was committed**: it is on show until the interrupt.
`late` counts frames the main loop missed (0 over long walks at both
speeds, PAL and NTSC); the loop waits for the frame after the last one it handled, not for
the next interrupt, or a build that runs past one costs a whole frame.

**The REU path** (`smooth.s`, after the copies; `ow-reu` only). With an REU the shift is two
transfers (the front screen out, then back into the back screen `off` bytes
along: what wraps round a row lands in the edge column, which is redrawn
anyway), colour RAM is stashed with it, and the flip's interrupt fetches
colour RAM back the same `off` along and colours only the new edge. The
interrupt then ends about 48 lines after it starts instead of 245, so colour
RAM is done before the screen starts on PAL and NTSC, and a walk leaves
about half as much time again free on PAL, three quarters on NTSC (idle-loop
turns, `--peek idle:4 --peek walked:2`). Each transfer in the main loop has interrupts off for its own setting
up and running only, since the interrupt shares the REU and `r_*`.
`screendump.py --vice=ARG` passes ARG to x64sc (`--vice=-reu --vice=-reusize
--vice=512`, `--vice=-ntsc`); the `-reu` goldens are the same screens through
the REU path.

The tiles are `assets/overworld.txt` (shared colours green, light green,
blue; each character's own colour 0-7), made into `src/lib/ow_data.s` by
`tools/mkoverworld.py`. Edges between terrains are drawn, not stored:
`sm_shores()` (under a second, once a land) fills `SHORE` at `$A000` with a
byte per square, a bit for each of its eight neighbours that gives it an
edge, and each quarter of a square picks one of four edges (a strip, an
inside corner, an outside corner where only the diagonal is) from the three
neighbours on its own corner (`ow_qvar`); `ow_blend` has the character for
every metatile, quarter and edge, so `char_at` pays one lookup for any of
them. Water gets beaches where land is; grass a rounded headland where sea is
on two sides; sand, swamp and mountain fray where they meet open ground
(grass and the places on it), cut by masks from their own art (`fringe`),
and the forest has its edges drawn (`edges`: a tree drawn back, a bush,
grass), since cutting trees only nicks them. Where two of those meet, the
lower frays toward the higher (`also`), which reads as a strip of grass:
sand against the rest, swamp against forest and mountain, forest against
mountain. Each terrain has two edge variants, picked by the same hash as its
art, so that a long edge wanders (a terrain drawn once is listed twice to
have one); what the asset draws is one quarter, mirrored (but never turned
upside down where the art has a top), and the generator refuses masks whose
edges would not meet. `ow_nflag` says what each tile is to its neighbours
(bit 0 land, bit 1 open, then one bit per `also` terrain) and
`ow_bsel`/`ow_binv` which of those bits make an edge on it.
**Places.** Stepping onto Millford, the castle, the Hollow Crown or home's
generated town goes in, built the game's own way (`town_data.c`'s maps,
`town_gen()`, `dungeon_build()`) into `local[]` and copied into `WORLD`, in a
tile set of its own: `assets/town_tiles.txt`, `castle_tiles.txt`,
`dungeon_tiles.txt` (sharing `places_common.txt`'s furniture, drawn `on
floor`: a `-` is the floor's pixel). `mkoverworld.py` turns every set into a
blob, a file on the disk (`t.town` ..., above), and `sm_tileset()` unpacks
one into the engine's tables in BSS and the character set; a set says its palette and what lies off the
map (`offmap`). Walls are two and a half dimensions: a top, and a face in the
lower half where floor is below, which is a drawn edge (`edges wall`, top
and bottom quarters drawn apart, not mirrored upside down). People are
sprites 1-7 (`sm_npcs()` places them from the camera just committed, the
interrupt writes them with the fine scroll; `passable()` keeps the hero off
them). Corners: the dungeon's rock rounds its outside
corners off into the floor and nicks its inside ones, and the floor darkens
against it (a dithered shadow, `edges floor`, with `also floor wall`); a
town's cobbles fray into the grass (`fringe floor`, the overworld's masks)
and its roofs round their outside corners, while the fronts and inside
corners stay square, because what is beyond them may be cobbles or a room's
floor rather than the grass the cut reveals. A terrain that is itself open
ground (a floor) takes edges only from its `also` list. The castle's moat
has banks of mud where it meets the grass and nowhere else (`shore blue
green against grass`: the water's own colour, the headland's, and the only
land that makes a bank), so its walls still stand in the water.
`sm_shores()` pads the map with the flags of the set's `offmap` tile, so
the castle's grass does not round off toward the map's edge as if it were
sea. **Roofs** (Ultima VI's): in a town or the castle a
building you are not in shows only its roof. A building is a closed area
with a door, found on the way in (`find_buildings()`: a flood from where
you stand is the outside, region 1; every other area another region; the
ones a door touches get roofs) -- in the castle that is the four towers and
the throne room. Its squares and its walls are drawn as `roof`, its middle
row as `ridge`, on codes $92/$93 that only the prototype's sets have (`code
roof 0x92`): thatch in towns (the walls' art and edges), slate in the castle;
its doors stay, its people are hidden. What you can walk on is read from
`local[]`, which has no roofs (`tile_at()`). Stepping into a building's
doorway (or onto its floor) takes its roof off and puts back the one
before; a door between two rooms keeps open the one already open
(`door_opens()`). Golden `openworld-door` stands in the inn's doorway. Only the box of what changed is redone:
roofs, `sm_shores_rect()` and `sm_redraw_box()` (the front copied to the
back, the box drawn over it, flipped to with its colour from the screen),
about 8 jiffies; `--poke whole=1` redoes everything instead, and the two
must give the same picture (they do, going in and out of the inn and into
the throne room). Goldens `openworld-inn` and `openworld-throne` are text
dumps, so they record who is visible. A town's way out is
its exit square; the dungeon's stairs go down and
up, and up from level 1 is out. The overworld is put away going in: in the
REU when there is one (`sm_reu_keep`, 8K, so coming out takes a third of a
second), otherwise rebuilt from the vault and `sm_shores()` (1.3 s). The
prototype defines the few of `game.c`'s and `actor.c`'s variables the
generators use itself, so neither is linked (with them came the view, the
sprite and the arms, 6K it has no room for). Going in takes about 0.75 s, the
dungeon 1.6 s (its generator). The goldens `openworld-town` and
`openworld-dungeon` (and their `-reu` twins) freeze mid-step inside;
`openworld-out` goes into Millford and out, and walks to where its people
would be if they had come too (they once did: nothing emptied `sm_npc_n` on
the way out). It is a text dump, not `--hex`, since only that shows sprites,
as `@` -- which is also how character 0 prints, so compare, do not count.
**A `.byte $2C` skip hides two bytes only**: one put in front of an absolute
load (three) ran the load's last byte as an instruction and drew wrong edges
in a few places without crashing; use a jump.

**`--poke seed=N` lands after the first land is built**, so it changes
nothing on screen until a rebuild: type `n` first (`seed=N-1`) to see seed N. `screendump.py --shot` follows the VIC bank, the
screen in `$D018`, multicolour characters, fine scroll and the 38/24 borders,
so it renders this as the C64 shows it. For tests: `--poke freeze_at=N`
(even, at speed 2) stops N pixels into the step after `freeze_after` steps,
at stage 3; type only the keys up to that step, since the harness waits for
the buffer to drain. `still=1` stops the water's animation for pixel
comparisons. The golden (`tests/golden/openworld`) freezes mid-step and
compares screen codes; `openworld8` does the same in the middle of a mixed
diagonal. Stage 2 drops back to 1 whenever a key is waiting, or
the harness, which types ten keys at a time, reads the screen mid-step.

## The multicolour arena

`make run PROG=owarena` (disk `dist/owarena.d64`): the game's fight on the
prototype's multicolour metatiles, with the party, the monsters and the shots
all sprites. Like the prototype it is a standalone experiment; the game links
none of it. Two parts, and the split is the point:

**The fight is a module** (`src/fight/`, `include/q4k/fight.h`,
`cfg/c64-fight.cfg`): one file, `FIGHT`, linked on its own with no BASIC stub
and no startup, loaded at `$5400` and entered by a jump at `$5800`
(`fight_call()`). Right after the jump is the block its caller fills in (the
`Fight` struct: the party, the monster's numbers with its name in place, how
many, whether the sides are a way out, a seed, and the names of two files);
`block.c` lays the same bytes down under the game's own names (`party`,
`potions`, ...) so that `arms.c` links unchanged, and `head.s` refuses to
link if the block moved or changed length (155 bytes: change both). The
module shares its caller's zero page and C stack (so the caller must be a
cc65 program, with BASIC switched out: its BSS runs past `$A000`), clears
its own BSS on every call and keeps nothing between fights; it returns
`BATTLE_*`, or `FIGHT_NODISK`, with health, magic, draughts and arrows in the
block as the fight left them. The pay, the levels and the spoils are the
caller's (`fq_foes` says for how many). `src/owarena.c` is only the menu and
the example of a caller; it stays below `$4000` (`cfg/c64-owarena.cfg`).
The module is 17.5K, 14K of it `fight.c` (`-Or`).

**What it loads for each fight**, itself, before it takes the screen:
- a field, `f.<ground>`, 3K at `$4000`: the finished screen (18 rows of
  field, a solid black row, six of spaces), the field's facts at `$4400`
  (`FI_*`: palette, where everyone starts, a boulder's four characters, a
  colour for each character, a flag for each square) and its characters at
  `$4800`, 128 at most. `tools/mkfields.py` draws it from `assets/fields.txt`
  (20 squares by 9) with a tile set of the prototype's, read by
  `mkoverworld.py` (`build()` returns the set as well as its blob), applying
  `smooth.s`'s edge rules itself -- so the 6502 has no tile code at all.
  Outdoors is `assets/field_tiles.txt`, which includes `overworld.txt` and
  adds a tree and a boulder; the lair and the dungeon room use
  `dungeon_tiles.txt`. The generator refuses a field whose sides are not shut
  but for the middle row, or where the monster cannot reach the party.
- the monster's sprites, `s.<name>`, at `$5000`: two frames, or eight for a
  2x2 body (Maldrath is four sprites). `assets/monsters.txt` has the art, in
  the hero's format (8 double-width pixels by 16; black, light red and its
  own colour, which is the bestiary's, so a cutthroat is a red bandit); drawn
  once, the second frame is the first sunk a row onto its feet. The party's
  three classes, the shots, the cursor and the blow's star are linked into
  the module (`fight_data.s`, generated, at `$5400`). `owarena.c`'s
  `sprite_of()` picks the file by the glyph the game draws the monster as.
`tools/mkfields.py --preview out.png` draws every field and sprite without
a C64.

**The multiplexer** (`src/fight/mux.s`; its head has the design). The fight
asks for up to sixteen sprites (`vs_x/xh/y/ptr/col`; one is on while its y is
not 0: members 0-3, monsters 4-9 or a big one's quarters 4-7, the shot 10, the
cursor 11) and `mx_plan()`, in the main program once a frame, sorts them by y
and gives the VIC's eight out in turn: the ninth on reuses the sprite given
out longest ago, on the raster line 22 past that one's y, if it stands at
least 3 lines below that. Squares are 16 lines apart and a sprite 21 tall, so
what cannot be shown is the ninth sprite on two neighbouring rows; then the
next plan starts its sort one sprite on, so a crowd flickers and nobody
vanishes (`mx_drops`, `mx_most`: six wasps and the party make ten with none
dropped). Two plans are kept and the frame's first interrupt (line 251)
takes a new one up whole. That interrupt also sets the first eight sprites
and chains to `$EA31` for the keyboard; the events follow down the screen;
the last (line 198) turns the background black for the text. **That one
needs no stable raster because row 18 is solid**: a character with every
pixel set shows no background, so nothing shows of where in it the colour
changes. Do not put text or art with background in that row.

**Text** is in the field's multicolour mode, where a colour of 0-7 makes an
ordinary character: so text has those eight colours only, and its letters
are the ROM's lowercase set copied to characters `$80-$FF` (`mx_font()`;
`t_ch()` turns PETSCII into them). Row 19 names the foe and lists their
health, 20-21 the party (a column of ten each, draughts and arrows in the
fourth), 22-24 the log.

**The rules are `battle.c`'s**, ported by hand, not shared: the flood, the
lone monster's banked turns, `LONE_HP`, `ARCHER_EYE`, the spells. Change one
and change the other. What differs: the field is 20 by 9, not 21 by 19 (so
two to four boulders, and long ranges mean less), there is no autoplayer,
and **it is eight ways**, as the open world walks: Q, E, Z and X (or the
stick's diagonals) step and strike diagonally; the flood goes
eight ways, a monster strikes from any of the eight squares round someone
(a big one from twelve places), and neither side squeezes diagonally
between two corners of wall. Shots fly eight ways too, the party's
arrows and spells and the monsters' (a big one shoots diagonally from its
corner), and stop where they would pass between two corners of wall
(`pinched()`). **A shot does not fly over the party**, as `battle.c`'s
does: whoever of them is in its way takes it (`loose()`, through
`bitten()`, at the attack it would have had), and the autoplayer will not
shoot through one. **Ground is slow**: each square's flag has a cost in
bits 2-3 (`FF_COST`: 10 tenths for grass, forest and floors, 15 for
sand, 20 for swamp, from `SLOW` in `mkfields.py`; the forest was 15 until
it was asked to be no slower, and the `thicket` scatter went with it); a monster spends it
out of its pace, and a member owes what a step costs past 10 (`mire[]`)
and loses a turn for every 10 owed ("is slowed by the ground"). **The
fields scatter more than boulders**: `assets/fields.txt`'s `scatter=`
names up to three more things drawn `on` the tile set's ground (in
`field_tiles.txt`, on codes `$92-$94` of that set's own: `oak` blocks,
`puddle` costs 20; `thicket`, 15, is drawn but no field scatters it), which `mkfields.py` writes as four
characters and a flag each at `FI_SCAT`; `scatter()` puts down two to four
things that block, boulders or trees, and one to three patches of slow
ground where it is quicker. **What those cost** (`balance.py --ow`, 40
runs; the autoplayer steps aside for a clear line): the keepers, fought
on floors that scatter nothing new, got harder for every party with a
ranged member, since their shots wait for the warrior to be out of the
way: W+A+Z drank 3.2, 3.5, 6.1 (3% lost) and 6.2 at the Gaoler, the
Drowned King, the Warden and Maldrath, against 1.9, 1.5, 4.7 and 4.0
before; three wizards won 42% against the Warden. A lone warrior and W+Z
hardly moved. The wanderers stayed easy (ogres the worst: 1.4 draughts
for W+A+Z, 2.7 for three archers). So **the party starts fanned out**
(`party_start()`, `form_x/form_y`): the leader two squares toward the
monster from the field's `@`, the second and third two squares either
side of it (a fourth between them), each falling back to an open square
round theirs, and nothing that blocks is scattered in that 5 by 3 box;
and **`ARCHER_EYE` is `5 + level / 2`**, two more than `battle.c`'s. With
both, W+A+Z drinks 2.6, 3.1, 6.5 and 5.0 at the four keepers and wins
every time (the game's own fight: 2.1, 1.5, 4.9, 4.6), and three archers
6.0, 8.8, 7.7, 5.7. The formation alone was worth about half a draught to
W+A+Z; the eye at 3, 4 or 5 moved W+A+Z by no more than the noise of 40
runs, and three archers by a draught and a half at the Gaoler.
**The arrow is one sprite block** (`SPR_ARROW`), which
`arrow_to()` fills for the way it is about to fly from `fight_arrow[]`:
eight pictures of 32 bytes, by `DIR_*`, that `mkfields.py` makes from
three drawn in `assets/monsters.txt` (`arrow`, `arrow_r`, `arrow_ur`) by
mirroring; `$5700-$57FF` (four blocks) are free. `owarena-arrow-ur` in
`tools/test.sh` checks the block after a shot up-right, and golden
`owarena-dragon` casts a spark up-left into Maldrath. **`tools/balance.py --ow`** measures
this fight: `owarena`'s `*` loads `FIGHTBOT` (the module built with
`-DFIGHT_BOT`: `battle.c`'s autoplayer, eight ways, which waits for no
frame) and fights the poked fight `auto_runs` times, counting into `res_*`
and the block's `rounds`, then probe stage 5. `--disk` points it at another
copy of the arena's disk, so variants of the module can run side by side.
`ARCHER_EYE` and `WIZ_GUARD` are `#ifndef` for such a sweep (`-D`).
**Measured against the game's own fight** (40 runs a fight, the keepers,
`balance.py` and `balance.py --ow`): eight ways, a lone warrior, W+Z and
W+A+Z drank what they do in the game, give or take half a draught, but
three archers drank up to 2.5 more (7.1 at the Gaoler against 4.6): a foe
that closes in diagonally reaches them as fast as they back off. And three
wizards against the Warden won 80% with 7.8 draughts, against 85% and 5.5.
**The tuning was then left as `battle.c`'s, on purpose** (until friendly
fire, above, made the archers' eye 5): the mixed party of
three, which most players will have, is what it should match, and the
extremes and the smaller parties are meant to be harder. An `ARCHER_EYE`
of `6 + level / 2` put three archers on the game's figures but made W+A+Z
about a draught easier than the game (1.2, 1.1, 3.2, 3.6 against 2.1, 1.5,
4.9, 4.6), and a bigger `WIZ_GUARD` lost more often (52 to 57% won against
the Warden at three quarters of the level and seven eighths: the
autoplayer counts it in deciding who is in danger, drinks later and dies
with draughts left) and made Maldrath easy at level 10. The wanderers are
no threat in either fight. The
music and the effects are the open world's: `ow_tune.s` and `ow_sfx.o`,
ticked by `mux.s`'s frame interrupt; the block says which song (none from
`owarena`, which loads no tune) and whether to carry on with it, and `M`
and SHIFT+`M` turn them off and on. **The open world's keys**, the engine's
and the fight's alike: Q E Z X diagonally (C does nothing), `M` the music,
SHIFT+`M` the effects (the fight's `next_key()` folds every other shifted
letter onto its plain one), and in a fight the up-arrow key (`$5E`, the
stick's button) shoots and `*` casts -- a wizard's up-arrow casts too, and
a wizard with one spell ready casts it without being asked which.
**Fleeing takes the whole party**: each member walks out of a side gap on
their own turn (`gone[]`, "fled" in their stats; off the field, their turns
skipped, no longer a target), and the fight is fled when the last one on
the field is out, or when all who stayed have fallen (`got_away()`). Then
`fq_foes` is how many were still standing, which `ow_fight.c` keeps for a
dungeon's monster in the game block's `fled` (1 + its index, and the count
in the top three bits), so the next fight with it is with those; won, or
the dungeon left for the world, it is cleared. **A level is shown in the
fight**: the caller passes `xp_each` and `to_level` (the block is 155
bytes now), and a win that reaches it flashes the party through the
colours to `SFX_LEVEL` (an arpeggio, `sfx.s`). A `.cmd` file types them as `\x5e`
and `\x2a`, since `*` would glob in the harness's `set -- $(cat ...)`. The bestiary moved out of `battle.c` into
`src/lib/bestiary.c` (still in the fight's engine) so that the arena has the
same monsters.

**Tests**: `screendump.py dist/owarena.prg --disk dist/owarena.d64 --mux
build/fight.lbl --poke auto_seed=7 --stage 3 --keys ...`. `--mux` reads the
sixteen sprites asked for instead of the VIC's eight registers (which hold
whatever the raster had got to), marks each by its square, and draws the
text rows on black in `--shot`; it also adds the module's labels to
`--peek`. The module sets the probe's stage to 3 while it waits for a key
with none typed ahead and 2 while busy; the menu's is 1, and 4 once a fight
has come back. `auto_seed` fixes the dice, `auto_hp` everyone's health. Keys
typed past the end of a fight go to the menu, where a letter starts another:
a test that waits for stage 4 must not type more than the fight takes.
`--keys=-a...` when the keys begin with a minus. For a picture from VICE
itself (the real raster: the split, more than eight sprites), build the menu
with `-DOWARENA_AUTO=7`, which goes straight into a fight, and run it with
`-limitcycles 200000000 -exitscreenshot`.

## The party

`actor.h`. Up to three made on the title screen (`party_make()`), a fourth
place kept for a computer-run companion (not written yet; the panel has room
for its two rows only by pushing the level and gold rows down onto the
place's, so that layout wants revisiting then). The gold, draughts, arrows,
`party_level` and `party_xp` are shared; health, arms (`weapon`, `armour`,
`bow`, indices into the lists in `arms.c`) and a wizard's magic and spells
(`known`/`ready` bits) are each member's. What a class may carry is
`gear_allowed()`, and "better" is by attack or defence, never by index: the
staff is after Dragonbane in the weapon list and the robes and hunting bow
were put in at index 1, so the shops' tier tables are offset for mail and
bows. Whatever a shop sells or a monster drops is `offer()`ed in `shop.c`,
which asks who takes it when more than one would be better for it. Home's
generated town always has the magic shop as its third building (`tgen.s`,
and `townref.py` with it), which sells robes and the spellbooks.

In a fight each living member takes a turn, then the monsters: the flood
fill starts from every member at once, so a monster goes for whoever it can
reach soonest, and `aim()` hits the first member in its line. A member at 0
health has fallen; `get_up()` stands them up on 1 after a fight won or fled.
**The balance** comes from the arena playing itself: `make` builds
`dist/arena.prg` with a second copy of the fight, `battle_bot.o`
(`-DFIGHT_BOT`), whose autoplayer plays the party; `tools/balance.py` pokes it
with a keeper or a group of wanderers, a party, its level and the arms of
that point in the story, runs dozens of fights four VICEs at a time, and
prints wins and losses, draughts and rounds. A lone warrior was the original
tuning (two or three draughts a keeper, five or six for Maldrath) and still
is; a party should be near it. What makes it so, all at the top of
`battle.c`: a monster that comes alone gets `LONE_HP` quarters more health
and `LONE_ATK` more attack for each member past the first (its health is
16-bit, in BSS2, so Maldrath's 250 can grow), and takes (n + 1) / 2 turns a
round, banked between the members' turns (`lone_bank`); an archer's arrows
get `ARCHER_EYE`; a wizard's wards turn `WIZ_GUARD` of each blow, their
spells grow with the level, and they get a point of magic back each round.
Measure again (`tools/balance.py`, about two minutes a fight at 40 runs)
after touching any of it, or a monster, or the arms. Three wizards and
three archers come out harder than a mixed party, by design: they are
the extremes.

## The joystick

Both builds. `src/lib/stick.s`
waits for a key or port 2, reading the stick once a jiffy (the debounce) and
repeating a held direction; directions come back as the cursor keys and the
button as `STICK_FIRE` (`$FE`, which no key types). `wait_key()` in `q4k.c`
and `next_key()` in `battle.c` call it; the map turns the button into `>`
or `<`, the fight into `F`. Menus, shops and the title still use `cgetc()`.
The tests never touch `$DC00`, so a stick-only path has no golden.

## Testing

**Do not run the full suite (`make test`) locally.** Push a branch and let
the GitHub Actions workflow (`.github/workflows/test.yml`, which runs on every
push to any branch) run it, and watch that result instead.

**A cloud session can run single tests itself.** `sudo tools/cloud-setup.sh`
first: with a GitHub token it installs CI's headless VICE 3.10; without one,
Ubuntu's VICE 3.7.1 with the ROMs taken from VICE 3.10's source tarball and an
`x64sc` wrapper that runs it on Xvfb (DEVELOPERS.md has the details). Then
`tools/screendump.py ... | diff -u tests/golden/NAME.txt -` checks one screen
in seconds instead of a CI round. On 3.7.1, CI's result is still the one that
counts.

Call `probe_stage(n)` at points worth asserting on, then
`make check` / `make test`. Goldens live in `tests/golden/<prog>.txt` and cover
the whole 40x25 screen, so any layout change shows up as a diff. Re-bless
deliberately, never reflexively.

**cc65's `rand()` starts from a fixed seed**, so without help every
playthrough meets the same monsters on the same squares — and the opening of
that sequence happens to be a long quiet one, which looks exactly like
encounters being broken.

Sampling a timer at startup is **not** a fix, though it passes a casual
glance. `srand(CIA1_TIMER)` at a fixed point after boot gave the same seed
two runs in four when measured: the machine is doing the same thing at the
same cycle on every boot. What is genuinely unrepeatable is how long the
player takes to press a key, so `wait_key()` counts the wait and
`rng_stir()` folds that plus the CIA timer into an accumulator that
`reseed()` draws from. `dungeon_build()` seeds deliberately, from the
dungeon, the level and the game's `lore_seed` -- the same level on every
visit, different dungeons in the next game -- so anything that calls it
reseeds afterwards, and a screen test that enters a dungeon must pin
`--poke lore_seed=N` or it meets a new layout every run.

`src/rngtest.c` measures `rand()`'s distribution after `srand()`; it is what
ruled the generator out when the encounter rate looked wrong.

**NPCs will invent geography** unless they are told where things are. The
land is made fresh for every game, so the host works it out: the C64 already
sends `lore_seed` with every request, and `host/geo.py` builds the same lands
with `tools/isleref.py` (cached per seed) and gives each NPC a compass point
and a rough walk to every place on their land, to the harbours, and to what
lies across each one. Nothing extra goes over the wire and the 6502 pays
nothing. `tools/mkgeo.py` writes who lives where and every name the host uses into
`assets/geography.json`, from `towns.txt`, `places.txt` and `names.txt` (the
lands, the towns a land makes for itself, their inns: `tools/mknames.py` turns
that file into `include/q4k/names_data.h` for `isle.c` and `town_gen.c`, so
the game and the host cannot disagree).
Change how the generator places things and the model changes with it, so the
NPCs stay right; `host/test_npcd.py` checks that `geo.slot_of` and the port
order agree with the game. Keep leagues and coordinates out of `assets/canon.md`
and the NPCs' lines: the host supplies the bearings.

**Measure before optimising on this machine; the intuitions do not
transfer.** Field of view clears the squares lit last turn, and the obvious
6502 move -- walk the array with a pointer instead of an index -- measured
*slower* than the indexed loop it replaced (27.1ms against 18.5ms), because
cc65 keeps the pointer out of zero page. What actually helped was not
touching all 399 squares at all: keeping a list of what was lit took it to
4.4ms. `--peek` plus the jiffy clock at `$A2` over 64 iterations is enough
resolution to settle questions like this in a couple of minutes.

**Disk access needs MEMSIZ raised first.** `$0283` points at the top of
BASIC memory, which is `$A000` on a stock machine — below this
game's BSS. Leave it there and KERNAL SAVE writes nothing and LOAD
walks into the weeds, both without an error you can see. `savegame.c` and
`modem.c` both raise it to `$CF00` before touching the drive.

**`cbm_save`'s last argument is a byte count, not an end address** — unlike
almost every other save routine on this machine. Passing an end address
produces a two-byte file containing only the load address, and a silent one:
the call returns success.

**Fights are rarer, and there is no `R`.** `encounter_roll()`
rolls out of 200, so the percentages in it are halves; the only way out of a
fight is the gap in a side (below). `L` where nothing is hidden raises the dead one time in
`grave_odds` (5; `--poke grave_odds=0` for never), except in towns and with
`no_encounters` set. In a dungeon each level counts as one more hero level
when `battle_wander()` picks what turns up and how many.

**The arenas have an exit on each side.** The middle row (`LOCAL_H / 2`) is
open at both edges and every other edge square is a wall, so walking off
the side at that row is a flight that always works (`can_flee` only: not from
a keeper or Maldrath), after however many blows the thing behind you has had
on the way. `scatter()` in `battle.c` puts eight to eleven boulders down at the
start of each fight, only where the square and all eight round it are open
(`open9.s`), so they can cut nothing off; not for Maldrath, who is two
squares across. Fights are therefore not the same twice, and their goldens
come from `rand()`'s sequence.

**A screen test that walks in the open is flaky by construction.** Overworld
and dungeon squares roll for wandering monsters, and the seed now comes from
the player's own timing, so the roll differs every run. Walk inside a town
(towns never roll), or pass `--poke no_encounters=1` to switch the rolls
off anywhere. This is what turned the save test red one run in several,
with a rat on the screen instead of the map.

**Every test that gets past the title screen makes a party first.** The
title asks a name, a class (`1`-`3`) and whether another will answer, so a
lone warrior is `Alaric\r1n` and the q4k case's three are
`Alaric\r1yMira\r2yZed\r3`. The party is made after the pokes, so a poke of
a member's health is overwritten: `--poke hp_start=N` starts everyone the
title makes on N health instead (the fight and inn checks use it).

**A test that has to be somewhere on the overworld walks there.** Home is
made from the seed when you step out of Millford, so there is no position to
poke in advance: start in Millford with `--poke quest_stage=1` (nobody leaves
before Garrett has talked to them) and `--poke lore_seed=N`, and type the way.
`tools/homepath.py SEED FROM TO` works the keys out from the model of the
generator (`--land D` for a continent, `hop` to step off a landing and back),
and `tools/mkcmds.py` writes the `.cmd` files that use it; the daemon tests in
`tools/test.sh` call it inline. Change the generator or the seed and rerun
`mkcmds.py`, then look at what moved before blessing. Pokes are applied at the
title screen, before the game starts, so `lore_seed` and `quest_stage` stay
(`game_init()` runs earlier still).

**People block movement**, so two NPCs in a line make the inner one
unreachable. `tools/checkmap.py` checks this; run `make test` after editing
`assets/towns.txt`.

## Testing: the harness

`tools/screendump.py --keys "..."` types into the KERNAL's keyboard buffer
(`$0277`/`$C6`) directly — VICE's own key injection proved unreliable — which
is how to drive turn-based input, and typed dialogue, without a human.
`--shot` renders a PNG from emulated memory (screen RAM + colour RAM + the
charset), so it can capture a screen mid-conversation; VICE's own
`-exitscreenshot` only fires on quit.

Note that `make check` opens a real VICE window that takes keyboard focus.
Typing into it while a test runs will change the result.

**Do not measure anything mid-fight from the probe page.** `publish()` only
runs between turns, so a battle still in progress never reaches it: an
encounter rate of about 8 percent read as a flat zero across 127 rolls, and
the bug was entirely in the measurement. `--peek enc_rolls --peek
enc_fights` reads the counters directly and gets it right. Those two are
deliberately non-static so they have labels.

**The hero is a sprite, not a screen code**, so screen RAM holds the
terrain under them. `screendump.py` puts `@` wherever an enabled sprite sits
in its text dump (a hi-res one by the middle of its pixels, a multicolour
one by the middle of its square, which its bob does not move), which is how
the goldens still record where the hero is standing; `--raw` and `--hex` show plain screen RAM. `--shot` draws sprites
too. Any screen that takes over from the map must call `sprite_hide()`.
The sprite is one member of the party at a time (`sprite_as()`): the leader
on the map, whoever's turn it is in a fight, where the rest are their class's
glyph (`K`, `A`, `M` in a dump). Each class has its own frames, in HIDATA.

**A `save` or `load` failure in `make test` is known flakiness: ignore it.**
Unless your change touches `savegame.c`, the disk, or what those two screens
show, do not rerun the suite to chase it, and do not try to diagnose it
mid-task. Sessions have lost a lot of time doing exactly that. Say in your
report that it failed and move on. A diff that shows your own change (a
moved wall, say) is not flakiness: re-bless that. But `make bless` can
catch a flaky run too (it has, twice), so read `git diff tests/` after
blessing; a save golden missing "Restored." should be hand-edited from
the committed one instead.

**Header dependencies are tracked** through cc65's `--create-full-dep`
(`build/**/*.d`), so regenerating `include/q4k/*.h` -- a new tile moving
every actor's code, say -- rebuilds whatever includes it. Before this,
stale objects quietly used the old values: new furniture drew in the
keeper's red, and villagers once drew as drawbridges. A build dir from
before the change has no `.d` files, so clean it once. The `HIMODS` caveat
under Memory still stands: flags are not dependencies.

**Open question: do three sprites upset disk access?** When the tunic went
in as a third sprite, the save/load tests failed 7 runs in 20 against 20 in
20 with two sprites -- but another session was editing and testing in the
same checkout at the time, so the comparison is not trustworthy. Recheck
with the machine otherwise idle, around 20 runs of each case. If three
sprites really do fail, hide them during disk access in `savegame.c`: every
enabled sprite takes CPU cycles from the KERNAL's serial-bus code.

`--peek SYM[+OFF][:WIDTH]` (or `$ADDR` for an address) prints any global after the run (wider than 8
bytes, just the bytes as hex, last first); `--poke
SYM=VALUE` (and `SYM+OFFSET=VALUE`) writes one before it. Both use the label
file ld65 emits next to the binary. Prefer it over adding debug
hooks to the game: the quest, the inventory and the party struct are all
reachable from a test this way, and none of it ships.

**The game needs its disk in every test past the title screen**, since it
loads its engines from it: the `.cmd` files pass `--disk dist/q4k.d64` (the
online cases swap in `dist/online/`), and the save tests start from a copy
of that disk, not a blank one. Without it the game stops after the title
screen with "Put the game disk back". **`make` relinks the game but does not
rebuild its disk**: after any change that moves the core (a library module,
say), `make d64 d64-online` before a single test, or the new PRG loads the
old engines and a test fails with no output (`sail` did). And never
`git stash` round a `make`: the objects built from the stashed tree are
newer than the sources that come back, and stay.

**`--fastdisk`** turns the drive emulation off and has VICE serve the
KERNAL's LOAD from the image (`+drive8truedrive` and `-trapdevice8`, or
`-virtualdev8` on a VICE before 3.9, which `trap_device8()` asks `x64sc
-help` about). Most of the open world's goldens and its cases in
`tools/test.sh` use it: a test that took 24 seconds takes 4, and all of
the open world's goldens went from 142 seconds to 59, four at a time. Those
whose point is the drive keep it: `openworld-talk` (a STOP during a LOAD),
`openworld-load`, `openworld-party` (F1 through the bootstrap), the
`openworld-save` case (a save and its scratch), and one overlay switch an
engine, `openworld-dungeon-out` (ow-cpu) and `openworld-town-reu`.

**`--reucache`** attaches a 512K REU that starts with the load cache full:
`build/owcache.reu`, which `make` writes (`mkowcache.py image`, laid out as
`ow_cinit.s` and the loader would leave it, with this build's stamp, so the
bootstrap keeps it), with `+reuimagerw`, so nothing is written back.
**`--cpu-engine`** (with `--splash` and an REU) has the bootstrap take the
REU for one whose transfers do not reach colour RAM (`OW_REU_BLIND`: the
byte at `$02F1`, `OW_TESTCPU`, which the harness writes while the splash
waits), so `ow-cpu` runs, caching. `openworld-reu` and `openworld8-reu` use
the full cache; `openworld-ale`, `-colour` and `-garrett` run `ow-cpu` with
it; `openworld-dungeon-reu` and `openworld-town-reu` start empty, for the
misses, and the rest have no REU, for the plain loads. `openworld-cache` in
`tools/test.sh` goes into Millford and out with an empty cache (hits on the
way out, `openworld-out`'s screen) and with `--reucache --cpu-engine` (no
misses). All the open world's goldens: 142 seconds before `--fastdisk`, 59
after, 51 with the cache; what is left is mostly the bootstrap (the splash
and building the lands) and the five that keep the drive.

**The modem.** `tools/screendump.py --rs232 PORT` wires the user port to a TCP
socket and drops warp mode. Without warp an emulated 1541 would take a minute
for each engine, so with `--disk` it also turns the drive emulation off and
lets VICE serve the KERNAL's LOAD from the image (`-trapdevice8`). `host/npcd.py --mock` serves deterministic canned
lines from `assets/npcs.json`, which is what the dialogue golden test uses —
never point a golden test at a live model.

**The dialogue backend** is a local Ollama model over its OpenAI-compatible
endpoint, called with plain `urllib` so `host/` has no third-party
dependencies. Prompts are assembled host-side and keyed by the id the C64
sends; the protocol carries no prompt text. Small models need the output
policed rather than requested — see `for_c64()`, which enforces the sentence
cap, strips stage directions and speaker labels, and clamps to ASCII. That
policing applies to model output only; authored mock lines pass through.

## Debugging a hang in VICE

A screen test that times out ("probe never reached stage N") says nothing
about why. VICE's binary monitor does, and `tools/screendump.py`'s
`Monitor` class (with `launch`, `connect`, `press_splash`,
`wait_for_stage`) is easy to drive from a throwaway script that does
`sys.path.insert(0, "tools"); import screendump as sd`. What has worked:

- **Where is it?** Registers are command `0x31` (body: one byte, 0): a
  count, then for each a size, an id and a 16-bit value; id 3 is PC, 4 is
  S. Sample PC a few times a second and name it from the `.lbl` file (the
  nearest label below). A static C function has no label, so assemble the
  module with a listing to place an address: `cc65 <same flags> -o x.s
  file.c; ca65 -l x.lst x.s`, then the module's CODE offset from the
  `.map`.
- **Breakpoints**: checkpoint set is `0x12` (start, end, stop when hit,
  enabled, operation: 4 exec or 2 store, temporary), so a store checkpoint
  on `$02-$03` watches cc65's C stack pointer and one on `$0001` the
  banking. Then step with `0x71` (step over, count). An exec checkpoint at
  each return point of `main()`'s calls finds which call never comes back.
- **The picture**: `--vicshot PNG` (display get, `0x84`) is what the raster
  drew; `--shot` and the text dump are drawn from memory and know nothing
  of raster splits.
- **Compare against the last commit** in a scratch `git worktree` built
  there: it settles "was this already broken" in a few minutes.

Things these found: the `make -j` race on `src/ow/NAME.s` (see "cl65
makes `foo.s`"); an interrupt arriving with the KERNAL banked out, which
reads its vector from the RAM under it and runs anything; the C stack
pointer past `$0400` in an engine (its stack is the tape buffer, down from
there), which `memcpy` then copies from forever. And one about the tests
themselves: a `.cmd` file is read with `set -- $(cat NAME.cmd)`, never
`eval`, which turns `\x03` into `x03` (no RUN/STOP) and `\r` into `r`.

To run only the open world's goldens, four VICEs at a time, loop over
`tests/golden/openworld*.cmd` doing `set -- $(cat $f); tools/screendump.py
"$@"` and diff against the `.txt` (about six minutes for all of them).
Don't edit `screendump.py` while such a run is going: the cases that start
afterwards load the half-edited file.
