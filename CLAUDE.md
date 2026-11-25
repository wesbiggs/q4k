# Quest for the King — working notes

Grid-based RPG roguelike for the Commodore 64 (Ultima III lineage): an open
world that scrolls smoothly over multicolour tiles. cc65 + VICE on macOS.
See DEVELOPERS.md for the workflow and README.md for the story and how to
play.

## Where to look

This file is long; search it for the **bold** lead-ins named here.

- **One game.** The open world (`openworld.prg`, `dist/openworld.d81`). The
  classic game it grew out of, which moved a square at a time (`q4k.prg` on
  a `.d64`, with an experimental networked build and a dialogue host), was
  retired: tag `v0.11.0` builds it. Many library modules are the classic
  game's, and their comments still say "the game's" this or that; the files
  they name (`battle.c`, `game.c`, `sprite.c`, `music.s` ...) are in that
  tag's history.
- **The open world**, in the order a session usually needs it: "The open
  world" (the disk, the bootstrap, the world, streaming, the REU, places and
  roofs); "The open world's engine is a core and three overlays" (`src/ow/`,
  the slot, `ovl.s`'s jump table, what fits where); "The text window, and
  `L`" (the two lines under the map, `sm_say()`); "The music carries on";
  "The screens of text are a program" (`owtown`: talk, shops, inns, party,
  help, and `ow_back()`); "Fights in the open world"; "What the party sees
  underground" (the dungeons' fog); "The open world's game lives in a
  block" (`OwGame`, saves); "The story in the open world" (gates
  and refusals). Keys are listed at the top of `src/ow/engine.c`; how the
  screen is built is at the top of `src/ow/smooth.s`.
- **Space** is the usual constraint: free bytes per region are in the open
  world's "Memory" paragraph and the engine's overlay paragraph. Read the
  `.map` after a change; `make` prints `owslots:` lines with the slot sizes.
- **The memory map** is a page, `docs/memmap.html` (checked in): every
  mode's blocks and what is free, drawn by `tools/mkmemmap.py` (what each
  block is) into the page's template, `tools/memmap-template.html`, from
  the linker maps, the disk's data files, `OWG_SIZE` and the engine's sprite count. `make` reruns
  it after any of those programs relinks; it stamps the date and the commit
  the build came from (and whether the tree had changes on top), refuses
  blocks that overlap, and rewrites the file only when an address moved, so
  the stamp is when the map last changed. A new segment or program is a line
  in `blocks()` there.
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
- A new hand-written source starts with `SPDX-License-Identifier:
  GPL-3.0-or-later` in its comment syntax; generated ones do not. The code
  is GPL-3.0-or-later (`LICENSE`), `assets/` CC BY-SA 4.0 (`assets/LICENSE`).

## The library's segments

Every program has its own cfg (`cfg/`): the bootstrap's (`c64-owboot`), the
engines' (`c64-openworld`), `owtown`'s, `owtalk`'s, the fight module's, the
arena's, `isletest`'s (`c64-flat`, everything in one load) and the lean
`c64-tool` (`owparty`). The library's modules are compiled once, so where
their code goes is a segment name chosen in the Makefile and placed by each
cfg:

- `HIMODS` (arms, panel, actor, quest) go to `HICODE`/`HIDATA`, and the
  `ENG_` lists to the segments named for the classic game's engines
  (`MAPCODE`, `TALKCODE`, `SHOPCODE` ...): the generators and the towns'
  tables, the bestiary, the talk and the inn, the shop and help. Changing
  either needs the objects rebuilt, because a target-specific `CFLAGS` will
  not invalidate them on its own.
- `SMALLMODS` (shop, inn, help, arms, talk, gift) are built with `-Or`, not
  `-Osir`: without `-Os` cc65 stops inlining shifts and the like, which is
  about a tenth smaller. Nothing whose timing was measured is on it.
- `dungeon.c`, `sfx.s`, `isle_gen.s` and `tgen.s` (with `town_gen.c` and
  `town_data.c`) are built again into the engine's own segments
  (`ow_dungeon.o`, `ow_sfx.o`, `ow_isle.o` ...), linked ahead of the library
  so that its copies are not.
- **The charset's spare glyphs.** A program with the game's character set
  (owtown, owtalk, owparty) has it at `$3800`, built at runtime
  (`charset_install()`), whose reverse-video half is mostly glyphs nothing
  draws, so three runs of it are free for read-only data: `CHDATA1` (112
  bytes, `$3C90`), `CHDATA2` (248, `$3D08`) and `CHDATA3` (448, `$3E40`),
  each a segment of its own. A module puts its rodata there with `#pragma
  rodata-name ("CHDATA3")` after its includes (inn and talk do; the land and
  town generators' tables are in `CHDATA1`). `$A0` is the input cursor and
  `$80-$91` the actors: check `assets/tiles.txt` before using any glyph above
  `$91` on screen.
- `local[]`, the town, dungeon or continent map, **is the font image** (2K of
  HIDATA, `src/lib/local.s`), reused once `charset_install()` has copied it
  to `$3800`. Install the charset before anything writes a map;
  `charset_install()` refuses a second copy, since by then it would copy the
  map. A continent is 64x31 of it, not 64x32. In an engine it is at `$8000`
  and in no file.
- **The RAM under the ROMs** (`src/lib/vault.s`). `$D000-$DFFF`, under the
  I/O area, holds home for the whole game, 64 by 64 bytes of tiles, which the
  bootstrap builds in place; `$E000-$E7BF`, under the KERNAL, the continent
  the engine keeps (getting in and out of all-RAM is `vault_bank.s`); the
  rest under the KERNAL is the music's, a tune's header at `$E7C0` and the
  tune at `$E800-$F5FF` (see Music), but for `$F600-$FBDF`, what the party
  has seen of the land (`OWG_WFOG`: "What the party sees of the land"), and
  `$FC00-$FEFF`, what they have seen of each level of the dungeon they are
  in (`OWG_FOG`: "What the party sees underground"). Reaching the vault takes the machine to
  all-RAM (`$01 = $34`), which takes the KERNAL and the I/O area away, so
  interrupts are off for as long as it lasts and the only code that may run
  is in ordinary RAM and calls nothing of the KERNAL's. An NMI cannot be
  turned off and its vector is read from RAM under the KERNAL too, so
  `vault_enter` (and the music, which plays with the KERNAL out) points it
  at `vault_nmi`, which banks the KERNAL in, runs its handler and banks it
  out again. **Write that vector with the KERNAL in** (the writes reach the
  RAM under it, and meanwhile an NMI goes through the ROM's): written with
  it out, an NMI between its two bytes (the engine's corners, at any time)
  jumps through half of each. That hung the way back from a fight, through
  `owc_xfer`'s low byte and `vault_nmi`'s high, into a JAM.
- **The probe page**, `$CF00-$CFFF`, is every cfg's, for the test harness
  (`include/q4k/probe.h`): don't place data there. Above what the probe uses,
  `$CF14` on is the disk icon's (below) in the engines, and `BSS2` in the
  programs that link the game's modules. ld65 does not clear `BSS2`.
- **`__RSBUF__`** (`$CD00`) is where owtown's, owtalk's and the flat cfg's
  BSS stops: the classic game kept the KERNAL's RS-232 buffers there, and
  nothing opens the modem now. The game block is at `$CE37` above it.
  Check `docs/memmap.html` before taking any of `$CD00-$CE3E`.

## Things that will bite you

**cl65 makes `foo.s` from `foo.c` in the source directory on its way to
`foo.o`, and deletes it afterwards.** An assembly module whose name matches a C
module's is overwritten and then deleted, with no complaint. The land
generator is `isle_gen.s` for that reason (the classic game had an
`isle.c` beside it), and the towns' is `tgen.s` (next to `town_gen.c`). The same goes for one `.c` compiled
twice: the open world's parts (`src/ow/*.c`, once for each engine) raced on
`src/ow/NAME.s` under `make -j`, and an engine linked the other's code or
did not assemble, with nothing said (it hung at start with the stack
pointer past its top). Those are now `cc65` and `ca65` through
`build/ow/NAME-cpu.s` and `-reu.s` (`OWPART` in the Makefile).

**cc65 can get a chain of `&&` and `||` wrong.** `d_use()`'s first try,
`k == '-' && t == DN || k == '+' && t == UP || t != UP && (...)`, compiled
to a `beq`/`bne` pair to the same label, so `+` on the stairs down did
nothing. Spell such tests out as nested `if`s, and read the `.s`
(`build/ow/NAME-cpu.s`) when a condition misbehaves.

**Screen codes are not ASCII and not PETSCII.** They index the active charset.
The screens of text run the lowercase/mixed set (`VIC.addr = 0x17`, i.e.
`$D018 = $17`) because cc65's `conio` emits codes for that set — leave it in the boot-default
uppercase set and every letter `cputs()` writes renders as line-art. In the
lowercase set `$41-$5A` are `A-Z`, `$01-$1A` are `a-z`, `$20-$3F` are ASCII 1:1,
`$A0` is a solid block.

**VICE halts the CPU whenever a binary-monitor command arrives**, including on
the initial connect. Every read must be followed by `MON_CMD_EXIT` (`0xAA`) or
the emulated machine never advances. `tools/screendump.py` handles this; if you
write another monitor client, remember it.

**cc65 re-encodes character literals to PETSCII at compile time.** `'a'`
assembles as `$41`, `'A'` as `$C1`, `"PING"` as `$D0 $C9 $CE $C7`, and `'\r'`
as `$0A`. This is correct for `conio`, which expects PETSCII — and wrong for
every byte that touches the screen directly, the files on the disk (the
talk's are ASCII), or the keyboard's matrix. It has caused three separate
bugs so far: garbled text output, a protocol that sent shifted PETSCII, and
range tests in `ascii_to_screen()` that compared ASCII input against PETSCII
bounds (uppercase fell through the lowercase branch and came out as
`$E1-$FA`). **Rule: any comparison or literal involving non-PETSCII data
uses numeric constants.**

**`$CF00-$CFFF` is reserved** for the probe page by every cfg. Don't place
game data there (bar what "The library's segments" says).

**Colour RAM is write-only-ish nybbles at `$D800`** and is *not* covered by the
VIC bank switch — it stays at `$D800` regardless of where the screen matrix is.

**The open world's game lives in a block every one of its programs leaves
alone**: `OwGame` (`include/q4k/owgame.h`) at `$CE37-$CEFF`, between the
engines' BSS (cut to end there) and the probe page: the party, the purse,
the quest, the looted bits and where everyone is (the area, the town or
dungeon and its level, the square, the world square, the ships, the boat,
the music, the effects and whether the party's lines are hidden, and the
dungeon monster last fled from with how many of it were left: `fled`;
and, last, the treasure maps carried, `tmap[]`). ld65 does not clear it. The engines' cfg names its party `party`
(it comes first), and they copy the rest in (`take_block()`) and out
(`owg_where()`, before a save or another program); a program that links
the game's own `actor.c` uses `owg_in()`/`owg_out()`
(`src/lib/owgame.c`). A save is this block as it is, in `ow-save` (201
bytes, `OWG_SIZE`: the whole of `$CE37-$CEFF`, so a new field must move the
bound first, or find spare bits as the pronouns did (below) -- `OWG_ADDR`, the engines' cfg (`_party` and BSS), `foes.s`'s
`FOE_A`, `stlow.s`'s base (its offsets are from the start, engine.c checks
them), `mkowsave.py`, and `tools/test.sh`'s `party+N` peeks if a field
moves; put a new one last, as the maps were, so that an older save loads
as the start of it (the bootstrap loads a save at `OWG_ADDR`, not at the
file's own address, and `save_version()` takes `OWG_MAGIC_V1` and `_V2`
with no maps) -- and the engines' BSS ends well below it; it is what
`tools/mkowsave.py` writes too and the engine checks): F1 in an engine writes it, after Y to "Save over the saved game? (scratching the old file first; the border is light blue while the drive works, red
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
the ship (8, 9); the king's crown and its bob are the town overlay's
(`OVTART`, `src/ow/town_art.s`, which `mkoverworld.py` writes from the
town's and the castle's sets' `sprite`s, with `_king_frame` its pointer),
since there is no room past `$5280`, and he is in yellow. Each member's colours are their byte of `tunic[]` in the
game block: the clothes in the low four bits (`OWG_DRESS`), the skin in the
high (`OWG_SKIN`; 0, as in a save from before, is light red), from the name
at first (`ow_party.c`'s `tunic()`: the clothes as sprite.c's `tunic_of()`,
the skin light red or brown at even odds, never the clothes' colour;
`mkowsave.py` and `owarena.c` have it too), and the hero is drawn in the
leader's (`sm_tunic`, `sm_skin`), a fight in everyone's (`ow_fight.c`).
**The skin is a sprite of its own**: a multicolour sprite has one colour of
its own, so the party's figures have their `s` pixels left out
(`figures.py`'s `bare()`) and a second sprite draws only those
(`skins()`): hi-res on the map (sprite 7, over the hero while on foot; the
leader's two frames copied from `ow_skins` in OW-DATA to `$7E80-$7EFF`,
just below the corners, so the people are multiplexed over 3-6) and on the
party screen (sprite 1); multicolour in a fight, whose multiplexer has
every sprite so (`V_SKIN`, 12-15, one frame a class at `SPR_SKIN`,
`$5200`, which the module copies from `fight_skins` as a fight starts,
since `$5400-$57FF` is full; a line lower on the bob, which `screendump.py
--mux` leaves out of a dump's marks, as the text dump does the engine's).
Villagers and monsters keep `$D026`'s light red. **The party screen**
(`I`, `party_look()` in `owtown.c`) shows one member at a time, with
`shop.c`'s rows (`party_head()`, `member_rows()`, `party_totals()`,
`party_prepare()`, which the game's own `party_draw()` is made of) and
their figure doubled below them: cursor left and right go through the
party, `1`-`3` puts the member shown in that place (`fig_swap()`: the
member and their colours; the first leads, and is the hero), `T` steps
the clothes' colour on (all but black; SHIFT+`T` back: `fig_step()`), `P`
round the pronouns (he, she, they, it), `S` prepares the spells of a
wizard who knows more than one, and `M` lists the treasure maps
(`tmap_list()`, `src/lib/ow_tmap.c`); RUN/STOP goes back to the game, and no
other key does. The skin, like the name and the class, is owparty's
alone. The figure is `src/lib/ow_figs.s` (generated, segment
`OWFIGS`, aligned to a sprite block in MAIN, which it asserts is where the
VIC sees RAM); room for it came from moving `charset_install()`
(`TITLECODE`/`TITLEDATA`) and the party screen's code to HIGH, which only
a fight overwrites and neither runs after one. Golden `openworld-colour`
is the screen, and `openworld-recolour` in `tools/test.sh` changes
Alaric's colour with `T` and checks the engine's hero has it.

**The story in the open world** is the game's: `owtown`'s conversations
hand over what `gift.c`'s `quest_after_talk()` says (Garrett's arms, the
Sigil, the keys, Dragonbane, the reward), with the quest stage and the
items in the game block. Its gates say no with a sound (`SFX_NO`,
`sfx.s`, once a press: `bumped` in `engine.c`) and why, in the text window
(`refuse()`): Millford's gate before Garrett has armed the party
(`t_can_step()` in `town.c`), a dungeon without its key (`to_dungeon_if()`
in `world.c`), and the king's ship before he lends it (`ship_held()`:
`IT_SHIP`, the free bit of `items`, which `gift.c`'s king gives a party
holding the Emberstone; no new quest stage, so the talk's stage ranges
stand). Golden
`openworld-garrett` asks Garrett for arms with the story at `Q_START` and
walks out. The quick start and the disk's save are at `Q_HEART` with every
key and stone and the king's ship, so that nothing is shut for the other
goldens. Golden `openworld-sail` walks from Millford to the ship and sails
east.

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
the game block's `quiet`. **What a win leaves** is the game's won():
arms from what goes armed (goblins, bandits to ogres, and natives drawn
as one; one in `gear_odds`, 12, shop.c's), and from a wanderer or the dead
on the overworld a treasure map (one in `treasure_odds`, 10, while one of
`OWG_MAPS`, 4, is free). `ow_fight.c`'s `spoils()` rolls them (MAIN) and
says "It left something behind."; the shop's code that hands arms out was
under the module, so `owtown` sets `OWB_SPOILS` and `ow_back()` loads
`owtown` again instead of the engine, whose `ow_spoils()`
(`src/lib/ow_tmap.c`, SHOPCODE) offers the arms through shop.c's
`gear_drop()` (`offer()`, asked who on the screen's rows) and puts the map
on a plain square of the land (grass, forest, sand, swamp: not a place, a
road or the hero's), read from the vault with `vault_peek()` (home at
`$D000`, the continent the party is on at `$E000`, the one the engine
keeps). A map is `OWG_TMAP(land, x, y)`, the land's own square. The party
screen's `M` lists them from the land's nearest place, as the game's
`treasure_maps()` did, and `L` on the square digs it up (`w_look()` in
`world.c`: 16 gold a quest stage past the second, plus `(x ^ y) & 15`, and
two draughts). `--poke loot_always=128` (the engine's, `OWF_LOOT` in the
fight's kind) makes a win leave everything it can: `openworld-spoils` in
`tools/test.sh`; `openworld-dig` pokes a map under the hero and digs. **At sea**: a step of
the ship's onto the deep sea rolls too (6 in 200, `wander()`; the shallows
are safe), and what comes up is `ow_fight.c`'s `sea_wanderers[]` (a sea
snake, a giant crab, a sea serpent and a kraken, by the party's level, with
sprites `s.serpent`, `s.seacrab`, `s.kraken`), fought by `seafight`
(`OWF_SEA`): `src/fight/fight.c` built a third time, with `SEA_FIGHT`
(the Makefile's `fight_sea.o`), on `f.deck`, the king's ship's deck
(`assets/fields.txt`'s `sea` field: the `bridge` tile's boards, red, in
`field_tiles.txt`, and `"` the deep sea round it). There the monsters keep
to the water: `roomy[]`, a big body's room elsewhere, is the sea's (0
where they may swim), which `flood()` and `foe_start()` use, and nothing
is scattered; the party keeps to the deck (`wall[]` as ever) and cannot
flee. `--poke enc_always=3` makes every roll at sea a fight and none on
land: `openworld-seafight` in `tools/test.sh`. Not
yet: pay for the foes killed before a flight.
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
right border of line `$E6`, from an interrupt on `$E4`, since one on
`$E5` often reached its write too late). Its letters are the game's own bold ones
(the screens of text's, `font_data.c`), as the lowercase set's first 96,
copied to codes `$A0-$FF` of the engine's character set by `font_copy`
from `ow_font` (`src/ow/ow_font.s`, OWDATA, so in COMMON: generated by
`tools/mkowfont.py`, which draws a coin on screen code `$5D`, a draught
on `$5E` and an arrow on `$5F`, PETSCII `$DD`-`$DF` in what the engine
writes), so
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
`battle_undead()`); never in a town or with `OWG_CALM`. On the
overworld a treasure map's square is dug up first ("Fights in the open
world"). RUN/STOP asks to quit (Y or N, in the window) and goes back through
the bootstrap with the world block empty, which shows the splash again.
`?` is `owtown`'s `OWB_HELP`, the open world's keys and the quest's next
step.

**The party's lines, the full screen, and the engine's code below $0801.**
The text window shows the party as a fight's panel does (name; health,
green over half, yellow down to a quarter, red under it; a wizard's
magic), and in the last column with room (fewer than four), a draught's
picture (purple) and their count, an arrow's (red: brown is 9, which
would make it a multicolour character) and theirs, and a coin's (yellow)
and the gold under them, each number right-justified (`numr` in
`stlow.s`, 16 bits for the gold), all
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
(`st_start()`: `game_turn` is 0; `mkowsave.py`'s save has a turn). Going
into a town, the castle or a dungeon from the world (or starting at a
gate) says "Entering NAME." after it: `t_enter()`/`d_enter()` set
`entering` unless resuming, and the core's `say_entering()` says it after
`st_start()` and `ov_switch()`, never with the window hidden. F1 and
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
`keeperbob`'s (`$94`) in the character set; his art (`dungeon_tiles.txt`)
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
leaves it paused for the screen after and the engine. **A load from the
REU's cache does not pause it**: the pause, and its twelve frames' wait,
are for the drive (VICE's sound buffer, the serial bus), and pausing round
every load (two at a town's gate) stuttered. So the engine's `disk_on()`
asks `ow_cached()` first and leaves the music alone for a cached file,
`ow_leave()` pauses without the wait, `owtown` and `owtalk` set
`owt_nowait` before the pause that goes back to the engine, and a talk's
file comes through the cache too (`talk_load()`, `owstand.c`'s).
Measured with the sound dump, a town's gate
went from two silences (241 and 343 ms) to none, and a talk from three to
the two program switches, about 150 ms each.

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
(1.2K free there, less the C stack), what runs once at the start above
it (the font image, `charset_install()`, `actor.c`'s and `town_data.c`'s
tables) with the portrait's code, and BSS at `$B000` as `owtown`'s (whose
talk buffer comes first, so that the fight module's BSS may run over it:
no inn follows a fight), so that VIC bank 1 is free for a portrait of the
speaker (below). Both
programs (and `isletest`) link `src/ow/owstand.o`, the few variables and
functions those modules use of what were the classic game's `game.c`,
`sprite.c`, `savegame.c`, `music.s` and `view.c` (as the engine defines
its own), ahead of the library; read `build/owtown.map`'s module list
after adding a module, in case it pulls in more than it should. `rand()`
starts from the same seed in every program, so `owtown` seeds it from the
game's turn (the engine counts its steps into `game_turn`) and seed, or
every visit would hear the same rumour. The rumours are the inns' own
files, which the talk already reads whole (a few hundred bytes
each): a REL file would let one rumour be read without the rest, which
nothing here needs. Golden `openworld-ale` buys a mug at Millford's inn. **The party's
making is a program of its own**, `owparty` (`src/owparty.c`, 10K, with
`cfg/c64-tool.cfg`: it all fits below the charset, the figures' sprite
blocks, `OWFIGS`, last), which the bootstrap
loads after F1 (`OWB_NEW`) or the tests' quick start (`OWB_QUICK`): the
story, then a warrior, an archer and a wizard with names picked for them
(and each name's pronouns, `name_pron[]`), each with a page of their own
with their figure, one key a thing (`C` the class, round the three; `N`
the name; `P` the pronouns, `S` the skin and `T` the clothes, each round
its choices, SHIFT back; DEL sends them home; cursor left and right go
through the others and `1`-`3` swaps them into a place, as owtown's `I`
does) (`src/lib/ow_figure.c`, `q4k/owfig.h`,
which owtown's party screen uses too), or one added while fewer than
three (`src/lib/ow_party.c`); RETURN puts the new game into the block,
saves it in `ow-save` as the engine's F1 does (so `openworld-party` runs on
a copy of the disk: `screendump.py --copydisk`), and loads the engine over
it at Millford. The engine
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

**The talk's portraits.** A person with a portrait (`PORTRAITS` in the
Makefile: the princess so far) talks beside it: `talk.c` draws and reads
keys through `include/q4k/talkscr.h`'s `ts_*`, which in the game are the
text screen's (`src/lib/talkscr.c`'s `tsc_*`, named `ts_*` by
`talkts.s`), and in `owtalk` `src/ow/talkpic.c`'s, which load
`v.NUMBERn` or `v.NUMBERp` (by `$02A6`; `tools/mkportrait.py` makes them
from `assets/portraits/NAME-n.v2i` and `-p.v2i`, version 6 hires
pictures at the top left, at most 24 by 16 cells, without their thumbnail
and metadata) at `$8000` and show it with vic2orious's `v2i.inc` in VIC
bank 1 (the library takes the bank as an option: 1 and 3 hold any
picture, 2 only a small or simple one, since the VIC sees the character
ROM at `$9000-$9FFF` there, where a picture's sprites would want to be;
3 is the game block's, the vault's and the music's in the open world), then draw the name and what is said beside it,
the topics below and the hint in row 24 with `v2t.inc` (`src/ow/picshow.s`,
`assets/vic2/`, the font the character ROM's, `src/ow/vic2/font.inc`).
Anyone else, or a portrait that will not load, gets the text screen. The
picture's code is `PICCODE`, which the file brings into bank 1 and
`owtalk.c` copies to `$BE00-$CCFF` before anything (138 bytes free there);
the file loads at `$8000` and stays, since its kernel runs in it
(`$8000-$AFFF`: 12K, so `mkportrait.py` refuses more). `v2i_show` clears
`$4000-$7F3F`, which takes what the file brought there, `npc_names[]`'s
letters (`town_data.c`'s MAPDATA) among them: `ts_open()` copies the name
first. While it is up the KERNAL is out and the picture's raster interrupt
runs: **nothing may call the KERNAL between `ts_ready()` (after the
person's lines are loaded) and `ts_close()`**; keys are `picshow.s`'s
`pt_key()`, the KERNAL's keyboard buffer if it has any (the tests type
there) or else the matrix (1-9, 0, RUN/STOP); the music is
`ow_play.s`'s `owt_music_frame()`, from `v2i_hook` (six in five frames
for a 60 Hz tune on PAL). The pictures' kernels write `$02`, so owtalk's
cc65 zero page starts at `$03`; `v2i.inc`'s is `$22`, `v2t.inc`'s `$2F`.
Lines are 8 pixels, a character row each, so that no two share a row's
colours; the words wrap by the font's widths and start a new page at the
bottom. The `openworld-portrait` case in `tools/test.sh` walks to the
princess, asks about her father and reads the colours of the question's
and the answer's rows (a text dump cannot read a bitmap), then leaves.
`--vicshot` shows it. The splash is still the kit's version 5: see
`assets/vic2/README.md`'s to-do.

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
turns in `SLOT` (`$5A00-$7E7F`, 9.1K, up to the hero's skin at `$7E80`
and the viewport's corners at `$7F00`), each a file of its own that ld65 writes
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
`ov_gen`, where a continent's build starts: the others' never run, and
`ov_use`, `+`, `-` or the stick's button: below), and an
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
is a sprite, multiplexed with the town's people's through `sm_npc_*` (with
`sm_npc_ox/oy`, pixels off the square, for the glide), one picture a kind
(`tools/mkfields.py` writes `src/ow/foe_art.s` from `assets/monsters.txt`,
`OVDART`, aligned to a block after the overlay's code, and asserted below
`$7F00`, where the corners' blocks start: only one frame was given room), however many
the fight brings. After each step of the hero's, each that sees them (no
farther than the screen, 10 by 6, and nothing but floor on the line)
comes for them at its speed in the arena (`Monster.speed`, tenths of a
square a turn, banked), one monster a frame in `ov_idle` while the raster
is above line 190 or the frame has only begun (`foe_room()`, through the
fog's `fog_left()`: a quiet frame's loop runs just after the interrupt at
250, which counted as no room once, so a monster took its turn only in a
frame busy enough to spill past it, and `openworld-foes` passed or not by
when the harness's keys came), gliding there at the hero's pace; one
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

**What the party sees underground** is the classic game's (its `fov.c`,
Rogue's model: tag `v0.11.0`): the room they stand in, walls and all (the
generator's `dun_rx`... rectangles, which are not all of the floor a room
seems to have), the squares round them, and straight along the four axes
(the corridors) to the first wall or secret door, which is seen too. What
has been seen stays on the map, as it is (the classic game drew it dim: a
multicolour tile has no room for a dim twin); what has not is the dungeon
set's `dark` (`$93`, solid black, not open ground, so rock beside it shows
no face and the floor darkens toward it), in `WORLD` only, as the blood is.
A monster out of sight is drawn as nothing (`foe_art.s`'s `foe_blank`, its
picture pointer swapped: `fog_foes()`), so one coming diagonally is seen
only when it is beside the hero or in their room. `src/ow/dungeon.c` decides
(`fov()`, `in_sight()`, `fog_level()`), `src/ow/foes.s` does the squares
(`fog_near`, `fog_rect`/`fog_lit`, `fog_dark`, `fog_show`, `paint_row`).
**What is seen is a bit a square** (`fog_seen`, 192 bytes a level), kept
for every level of the dungeon at `OWG_FOG` (`$FC00-$FEFF`, under the
KERNAL, where a write reaches the RAM: `show` writes it straight through,
and `fog_level()` reads the level's back with `sm_vcopy`), so that a level
left by the stairs, and the way back from a fight (the engine is loaded
again), are as far explored as they were. The game block has no room for
it, so a save does not keep it: the bootstrap clears it (after the splash,
whose picture is there), and so does the way in from the world, as it does
the monsters. **The level as built** (`fog_drawn`, 48 by 32, with the
chests drawn open and the blood: `muster()` comes first) is what a square
is drawn as once seen; it and `fog_seen` are `OVDBSS`, the dungeon
overlay's scratch after its art, in no file. **No frame is spent on it
when a step ends**, which starts the next and has no room to spare: what
the hero will see is worked out when the step starts (`d_can_step()` sets
`fog_due` for the square it goes to) in the first of its frames with
`FOV_LINES` (60) raster lines left (`fog_left()`), into a box of what is
new for the squares round them, one for each ray and one for the room;
the room is lit a row at a time (`fog_lit()`, entering it is a few hundred
squares), and what is new is drawn a row at a time too, as much as each
frame has room for: first its edges (`sm_shores_rect`, a square row at a
time, three lines a square), then its characters (`paint_row`, eight lines
a character: six measured), the new squares' and the one round them that
their quarters' corners change. Into the back screen once it is past its
shift for a flip still to come (the core's `phase`, and the camera not yet
two characters on from where the step began, `cam_x/cam_y`: the back is
the map a character on), which the
flip colours from the screen (`sm_fromreu` 0 tells the REU's engine so);
into the front, with its colour, while the hero stands still (and then
`pre_dir` is forgotten). Rows off the screen are left to the scrolling. The
rays step a pointer and a bit along rather than working each square out
(they had cost up to 150 lines). Measured over 64 steps down two levels of
the Hollow Crown (`late`, against the same walk before): PAL 1 (0), NTSC 6
(2); the REU's engine 0 (0) and 1 (0); and the screen, after any of five
walks on both engines, both video systems, is the same as a build that
draws everything again after each step. Goldens `openworld-dungeon` and
`-reu` are the first room lit and the corridors from it, the rest dark.

**What the party sees of the land** is the dungeons' fog out in the
open (`src/ow/world.c`'s "What the party has seen of the land" and "What
the hero sees", and `src/ow/wfog.s`): a square never seen is the overworld
set's `dark` (`$94`, solid black, not land and not open ground, so nothing
takes an edge toward it) in `WORLD`, which is what is drawn and what
`tile_at()` reads; the open sea between the lands is always seen. **What
is seen is a bit a square**, home's 512 bytes and each continent's 248, at
`OWG_WFOG` (`$F600`, under the KERNAL, written straight through), with a
copy of home's and the kept continent's in the engine's BSS (`wf_ram`,
fetched by `w_enter()` and `wf_take()` from `settle()`), from which the
streaming darkens each run it copies in (`wf_dark_run()`, `wfog.s`'s
`wf_dark`). Not in the game block, so not in a save (the bootstrap clears
it, as it does the dungeons'). **The look** (`wf_row()`, `wfog.s`) goes
from the hero out to 12 squares across and 8 down (two past the screen,
so that a step's new column has been looked at before the scrolling draws
it), a quarter at a time and a row at a time out from the hero: a square
is seen if the one before it on the way back (across, down or the
diagonal, by the slope) was seen and is not a mountain or forest, and the
hero's own is seen through; so a range hides what is behind it, and in a
forest only the trees round the hero are seen. A square seen for the
first time gets its tile from the row as the world keeps it (`wf_fill()`,
one copy a row, at its first such square), its bit, and the boxes grown
round it: the edges' (`sm_shores_rect()`, as a band worked through
`EQ_CHUNK` squares at a time in `w_idle()`, before the look goes on: about
fourteen raster lines a square, so a row of new land cannot be done at
once; none for the deep sea, which is to its neighbours what the dark
was), and, if on the screen, what to draw (`wf_draw()`, into the screen
on show with its colour, as the dungeons' fog draws while the hero stands
still, and `pre_dir` forgotten). The look runs in `w_idle()`, a row while
the frame has the lines a row has taken (`wf_lines`, learned); one started
while walking covers only the quarters ahead (`wf_ahead()` in `w_after()`),
and standing, all four. At a step's end only the squares round the hero,
and only if one is still dark (`wf_step()`: a step must be able to go
onto them, and **nothing may be added to the frame a step ends in**: a
few thousand cycles of bookkeeping there cost a missed frame a step), are
seen there and then, their edges worked out and drawn. **What it costs**
(`late` over `openworld-sail`'s 101 steps, all on land and sea never
seen, PAL): about 27 on OW-CPU (1 without the fog) and 22 on OW-REU (1),
nearly all at step ends beside something that hid what is now seen;
walking back over what has been seen, about 9 in 58 steps on OW-CPU and 1
on OW-REU. Not measured on NTSC. Drawing those step ends' squares over
the frames after (into the back screen too, as `foes.s`'s `fog_show` does
underground) would be the next saving. **Room**: the world's overlay was
full, so what is in `wfog.s` but `wf_draw()` -- the row loop, the bits'
lookup, the darkening and the boxes -- is in `OWDATA`, in `ow-data`, the
file both engines load: it may use only zero page, its own variables and
fixed addresses (`WORLD`, `OWG_WFOG`), and reaches back through vectors
the overlay sets as it starts (`wf_hitv`, `wf_fillv`, `wf_ramp`: world.c's
`wf_hit()`, `wf_fill()` and `wf_ram`, whose addresses differ between the
engines); the words world.c says, and the land generator's tables (under
`OW_WORLD`), went there too. `--poke wf_sync=1` (the tests', in the
overworld goldens) does each step's look all round at its end and draws
it, so that a screen does not depend on how much of a frame the look had.

**The rounded viewport.** Every map screen of the open world (the
overworld, towns, dungeons, both window and full screen) and the arena
has its four corners rounded off: black outside a quarter circle of three
squares' radius (48 pixels), drawn by sprites, since characters would move
with the fine scroll. Two hi-res black sprites doubled both ways (48 by
42), real sprites 1 and 2 in the engine and 0 and 1 in the arena (the lower
the number, the nearer: they hide anyone walking into a corner), are the
top corners and then the bottom ones. In the engine (`src/ow/smooth.s`,
"The viewport's corners") the move is an NMI, CIA 2's timer A, started
one-shot by the flip's interrupt each frame for about line 110 (a raster
interrupt cost twice the frames on NTSC); it is not started while
`sm_sproff` is set, which `disk_on()` does before every load and save,
because an NMI breaks into the KERNAL's serial transfers. Their pictures
are drawn by `sm_init()` into `$7F00-$7FFF`, which no file brings and no
overlay may reach. The hero keeps sprite 0 and their skin 7; the people
(townsfolk, ships, a dungeon's monsters) are multiplexed over 3-6:
`sm_npcs()` sorts them by y (`mx_plan`) and the first four take 3-6 from
the top of the frame, a fifth and sixth the first and second's sprites
22 lines below them (an event, `irq_mid`, irqstep 3), if they stand 25
below; one that cannot is left out of that plan (`sm_mxdrops`), and the next starts from
the next person, which the engine asks for every fourth frame while
anyone is (`sm_mxwant`): a crowd on a line takes turns. Nothing writes the
VIC's registers for a person any more: `sm_npc_ptr[]` and `sm_npc_col[]`
(engine.h) say their picture and colour, and `sm_mxdirty` that one
changed. `screendump.py` reads the plan's list (`sm_vx`, `sm_vxh`,
`sm_vy`) rather than the VIC's eight, marks no corner, and draws all four
in `--shot`. In the arena (`src/fight/mux.s`) the plan has six real
sprites for its sixteen, and the corners go down at line 100 among the
events. **What they cost**: the VIC's sprite fetches for two sprites on
84 lines, about 600 cycles a frame, which the CPU's scrolling on NTSC
cannot spare: missed frames (`late`) over `openworld-sail`'s 59 steps went
from 2 to about 26 there (with the corners on but not shown, 5); PAL 0 to
1; the REU's engine stayed at 0 on both.

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
`owtown`'s, past its talk buffer at `$B000`), the talks' and inns' files
(`talk_load()`), and the bootstrap's `ow-data` and tune. Where things
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

## Music

**SID files, one scene at a time.** Every tune is a standard SID file (PSID):
a 6502 player and its data, with an init routine (song number in A) and a
play routine called 60 or 50 times a second. `assets/music.txt` maps each
scene (`home`, `isle`, `town`, `castle`, `dungeon`, `sea`, `fight`, `boss`,
`talk`, `inn`, `shop`) to a SID file and one of its songs; `tools/mksid.py
map` makes that into `include/q4k/music_map.h` (`MUSIC_HOME` ...) and
`src/lib/music_map.c`. The engine picks the scene from where the hero is
(`play_for()` in `src/ow/engine.c`), the fight from what it is
(`fight_music()` in `src/lib/ow_fight.c`); a scene whose file is not the one
in memory loads it from the disk (`m.NAME`), and the same file and song
carries on. "The music carries on", above, is how it crosses programs.

**Where it lives.** One tune at a time, in the RAM under the KERNAL: the disk
file loads at `$E7C0`, a 64-byte header that `mksid.py` writes (init, play,
song count, the volume to restore after a pause, a byte a song for its
speed), then the tune at its own address, which must be inside
`$E800-$F5FF` (3.5K: past it is the land's fog, `OWG_WFOG`, and the
dungeons', `OWG_FOG`; the theme is 470 bytes). The engine's player is `src/lib/ow_tune.s`, called from
its raster interrupt; the screens of text play it on from the KERNAL's
interrupt (`src/lib/ow_play.s`), with the KERNAL banked out (`$01 = $35`)
and back in after; see the RAM under the ROMs for the NMI that this needs.
**The KERNAL's interrupt is 60 Hz on PAL as well as NTSC** (its CIA timer is
`$4025` on PAL, `$4295` on NTSC; measured: 151 jiffies in 125 PAL frames): a
song made for the frame rate (PSID speed "VBI") is called five times in six
on PAL, 50 Hz, and a "CIA" one every time.

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
few blocks on the disk.

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

**Pausing.** The disk code pauses the tune round its transfers, which the
interrupt would steal cycles from (`ow_tune_pause()`, and the talk's
`music_pause()`, which is `ow_play.s`'s). Pausing stops the calls, zeroes the
master volume and leaves the gates alone, so the notes carry on where they
were; it also *waits twelve jiffies*: with the drive emulated, VICE makes no
new sound while the drive works and replays its last audio buffer, so a note
sounding at the moment the disk starts is held for as long as the disk takes
(a five-second drone on a save; the SID itself had been told to stop, as
`-sounddev dump` shows). Silence has to reach that buffer first.

**Listening and measuring.** `x64sc ... -sound -sounddev dump -soundarg
out.dump`, through `tools/screendump.py --vice=...` (its `+sound` comes
first, so `--vice=-sound` overrides it), writes every SID register write
with its cycle delta in warp mode: that is how the SID theme was checked
against the old player, note for note and frame for frame. (Without warp
the dump writes register snapshots instead.) To hear it: `-sound
-soundrecdev wav -soundrecarg out.wav` with the game past its title screen,
`-warp` and `-silent` stripped (or `-sounddev wav -soundarg out.wav` if
VICE's coreaudio device is unavailable). `--peek ow_tune_on` says whether
it is playing; the tune's own variables are under the KERNAL.

Sound effects are on voice 3 (`src/lib/sfx.s`, built into the engine and
the fight as `ow_sfx.o` with `OW_SFX`), from the same interrupt, and work
whether or not the music is on (with a tune playing they leave the volume
register to it). `M` turns the music off and on, `SHIFT+M` the effects.
`sfx_step(tile)` is a step, a sound by terrain from a table indexed by
screen code, with the two feet at slightly different pitches;
`sfx_play(SFX_*)` the rest. Each sound is a short table of steps in `sfx.s`
-- frames, waveform, pitch, pitch change per frame. Two things about the SID cost an afternoon.
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

**The shallows.** Last of all (`shoal`/`scan` in `isle_gen.s`, `shoal()`
in the model), the sea round every land becomes shallow water: five passes
over the map but its edges, each making a square of sea shallow if one of
its eight neighbours is land or shallows from a pass before (the first
always, the rest three times in four), then a pass that fills a deep square
left between shallows; so they are one to five squares wide, and ragged.
Shallow water is `T_WATER` (with the beaches, lighter: more cyan); what is
left is the deep sea, `T_DEEP` (`$95`, the overworld set's own code, which
`isle.h` defines for C), as is the open sea between the lands (`load_run()`
fills with it). Its art is the water's with dark troughs, and it has no
edges: the shallows end square by square. A ship sails both; on foot
neither. The generator's thread keeps only sreg, ptr1, ptr2, ptr4, tmp1
and tmp3 across a yield (`ow_gen.s`), so the pass uses `at` (ptr4) for the
row below, not ptr3. The deep sea rolls for what comes up out of it
("Fights in the open world").

Home is 64x64 and has more land (a longer walk) and two kinds of ground, plains
or forest; a continent is 64x31 in `local[]` and any of five. A *special*
square (a landing, a town, a castle, a dungeon, a lair) is an even `G_` value,
so as far as connectivity goes it is not there: stepping onto it does
something, so nobody walks *through* one. Placing one has to leave the rest
joined and itself reachable, which is what keeps that true (a landing is
written out as sand, with no tile of its own, so the ship moored beside it
is what marks it): the landings go in
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
(`slot_of()` in `src/lib/slot.c`): slot 0 is Wardlow, 1 the
Drowned Stair, 2 Ashcombe, 3 Blackpit, which is the order the quest wants
them in. The king's ship sails once he has lent it (`ship_held()`, "The
story in the open world"); the quest's lines name the way to each land in
turn. Home's places are `home_fx/fy` (copied out of `isle_fx/fy`
when it is built, because building a continent overwrites those) and a
continent's are `isle_fx/fy`; the bootstrap hands them to the engine in the
hand-off block.

Every land has a town of its own, made by `src/lib/tgen.s` (in C it came to
1.8K): an inn, a shop (a smithy, an armourer or a bowyer), a third building
(a potion shop, or an empty house small or large) and up to two houses with a
villager in, on a grid of six lots with a street between, each building
bottom-aligned in its lot with its door in the bottom wall, so nothing overlaps
and every door opens on a street by construction. The keeper stands just
inside the door and what the building sells is on the corners of its top row.
Outside, in the street, each but a house has its sign (`signinn`,
`signweapon` ... in `tiles.txt`), diagonal from the door on the side the
door is nearer, which is always in the lot's columns, so it never blocks a
street that runs across.
`tools/townref.py` is the model, and `checkisle.py` checks all of that and that
the C64 builds the same towns. The ground and the scenery in the lots follow the
land's biome. Home's biome is `home_biome`.

The bootstrap builds home into the vault and each continent once, for where
its places and its landing are; the engine builds a continent again into the
continent it keeps when it comes near (below).

## The open world

The open world (`make run`) is a world from the land's generator, walked
with a camera that glides a pixel or two a frame over 16x16 multicolour
metatiles (2x2 characters), with the TeensyROM+ and an REU in mind.

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
the matrix) is the old quick start: a lone warrior, Alaric, with every
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
that the engine's file does not carry 5K of empty screens to the disk. The overlays are 5.9K (world), 7.5K
(town) and 7.1K (dungeon, its monsters' pictures at the end, and after them
1.7K of the fog's scratch, in no file) of the slot's
9.25K (`make`'s `owslots:` lines). Free, OW-CPU's (OW-REU has a little more), measured from the maps: 417
bytes in MAIN (the generators went to their overlays, and the REU cache's
loader came in, and then the corners' multiplexer, 1.4K; the engine no longer links cc65's `conio`: its one message is
written to the text screen's top row, and the keyboard's buffer is emptied
with a poke of `$C6`), 13 in OWLO (`$0430-$07F7`, below the BASIC stub: `stlow.s`), 19 in
OWTOP (`$5280-$53FF`), 125 in HIGH (`$5400-$59FF`, where the text window's
routines, stlow.s's helpers and the engine's words are), about 100 beside the
world's overlay (which has `wander()`, the overworld's encounter roll, out
of the core, and the land's fog: what more it needs goes in `ow-data`, as
the fog's did), 1426 beside the town's (for its maps to grow into), 384 beside the dungeon's (the fog took the rest: its scratch, 1.7K, and its code), 1229 in BSS, and 2.2K in COMMON (`$8800-$9FFF`, less `OWCBSS`'s 164 bytes: the cache's scratch and the multiplexer's plans; the text window's font, 768 bytes, is there too). String
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
(that was 37 missed frames a voyage), and does not start the switch,
four lines with interrupts off, between `$D6` and `$E5`, where it held the
text window's second interrupt past its line and a raster line of the map
showed over the party's names. And **nothing may be added to the
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
for a place finishes the queue first. **Ships**: one, the king's (`SHIPS` is 1; the game block
and the hand-off keep room for eight: `ship_x/ship_y`),
moored by the bootstrap beside whichever of home's four ports is nearest
the castle, the continents' docks having none (a landing is a square of
sand: there is no port tile).
Stepping onto it boards it (`boat` is its index + 1; the hero's sprite
becomes the ship, frames 8 and 9); in it you sail open water and step off
onto any land, where it stays; its position is kept as you sail. A party that falls wakes in Millford with the ship back at its
port (`G->ships = 0`, `ow_fight.c`). The quest's lines (`quest_line()`,
`quest.c`) say which way the next land lies: an `@` in `quest_task[]` is
the compass point of the continent whose `OW->slot[]` is the stage's
(`Q_EMBER` is Wardlow's), and `QT_NO_SHIP` is the Emberstone's line
before the king's leave. The continents' places go in as they do in the game
(the bootstrap works out `slot_of()` for each).
**The music** is the game's: the same SID files at the same place, and
the game's scenes (`assets/music.txt`: home, isle, sea, town, castle,
dungeon), through `play_for()` in `engine.c`. `src/lib/ow_tune.s` is the
player (the classic game's `music.s` without the effects), called from the raster interrupt
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
**Roads** join Millford, the castle, home's own town and its four ports in
one network, laid on home by the bootstrap right after building it
(`ow_roads()`, `src/lib/ow_roads.s`, using `$4000-$6FFF` for its grid and
queue): Prim's algorithm on the grid, a breadth-first search from all that
is joined to the nearest place that is not, and the way back laid as road,
straight where it can be, on grass, forest, sand and swamp only, never
through a place, and onto each landing it reaches, so that it runs down to the
ship moored there. `tools/roadref.py` is the model, draw for draw, and
`openworld-roads` in `tools/test.sh` searches it over every seed and holds
the C64's home (read from the RAM under the I/O area) up against it. A road
is one tile, `$96` (the overworld set's `code road 0x96 pass`, in no other
set and not in `tiles.txt`, so the game never sees one): its shape --
straight, turn, crossroads, fork -- is its edges (`edges road`), drawn where
a neighbour is not road nor a town or castle (`join`, a neighbour flag
bit of its own), and it is open ground, so forest thins and sand frays
toward it. It rolls for wanderers half as often as grass (4 in 200, `wander()`). The overworld set is now
at 158 characters of the most it may have, **160** (the deep sea took
four, and gave three back as one still character when the dark came): the road's interior is the
sand's art, and its edges took the room of the mountain pass and the
bridge, which no land of the open world has (a square of either would draw
as grass). Continents have no roads yet.
**Places.** Stepping onto Millford, the castle, the Hollow Crown or home's
generated town says its name and "(+ to enter)" (`w_arrived()`, from
`place_here()` in `world.c`, which has its own copies of the places' names:
the towns' and dungeons' are in their overlays); `+` or the stick's button
(`use()` in `engine.c`, from `typed()` or `fire`, then the overlay's
`ov_use()`: `w_use()`, `d_use()`, nothing in a town) goes in, and the way
out leaves the hero on the place's own square. A dungeon's stairs say which
way they go, and `+` goes down, `-` up (`d_use()`; the button whichever they
are). The tests type the `+` after the step onto the place. A place is built the game's own way (`town_maps.c`'s maps,
`town_gen()`, `dungeon_build()`) into `local[]` and copied into `WORLD`, in a
tile set of its own: `assets/town_tiles.txt`, `castle_tiles.txt`,
`dungeon_tiles.txt` (sharing `common_tiles.txt`'s furniture, drawn `on
floor`: a `-` is the floor's pixel). `mkoverworld.py` turns every set into a
blob, a file on the disk (`t.town` ..., above), and `sm_tileset()` unpacks
one into the engine's tables in BSS and the character set; a set says its palette and what lies off the
map (`offmap`). Walls are two and a half dimensions: a top, and a face in the
lower half where floor is below, which is a drawn edge (`edges wall`, top
and bottom quarters drawn apart, not mirrored upside down). People are
sprites, up to seven (`sm_npcs()` places them from the camera just
committed, and the multiplexer gives them sprites 3-6: "The rounded
viewport", below; `passable()` keeps the hero off them). Corners: the dungeon's rock rounds its outside
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
row as `ridge`, on codes $92/$93 that only the open world's sets have (`code
roof 0x92`): thatch in towns (the walls' art and edges), slate in the castle;
its doors stay, its people are hidden. What you can walk on is read from
`local[]`, which has no roofs (`tile_at()`). Stepping into a building's
doorway (or onto its floor) takes its roof off and puts back the one
before; a door between two rooms keeps open the one already open
(`door_opens()`). Golden `openworld-door` stands in the inn's doorway. Which
region each square is in is `region[]`, 42 by 27 (the biggest town with a
border of 0, so that a square's eight neighbours are always there), in
`OVTBSS`: the town overlay's own scratch, after its code in the slot, made
again on every way in. A wall is under the roof of the first roofed building
among its eight neighbours, worked out as it is drawn (`wall_roof()`, which
cost 2 to 4 jiffies a roof change against the two tables of owners it
replaced, 800 bytes of BSS). Only the box of what changed is redone:
roofs, `sm_shores_rect()` and `sm_redraw_box()` (the front copied to the
back, the box drawn over it, flipped to with its colour from the screen),
about 10 jiffies at the inn's door and 16 at the throne room; `--poke whole=1` redoes everything instead, and the two
must give the same picture (they do, going in and out of the inn and into
the throne room). Goldens `openworld-inn` and `openworld-throne` are text
dumps, so they record who is visible. A town's way out is
any square of the `exit` tile (`x` in `towns.txt`, `$97`, past the full
terrain block in the `$92-$9F` the sets' own codes use, so no set may take
it): not a door, so it joins no building and gets no roof, and a set with
no art for it draws its `fallback` (a tile a set has no art for is its
fallback to its neighbours too). A map with more than 16 tiles packs in
`rle.py`'s wide form (5 bits of index, runs of 8). The dungeon's stairs go down and
up, and up from level 1 is out. The overworld is put away going in: in the
REU when there is one (`sm_reu_keep`, 8K, so coming out takes a third of a
second), otherwise rebuilt from the vault and `sm_shores()` (1.3 s). The
engine defines the few of the game's variables the generators use itself
(as `owstand.c` does for owtown), so `actor.c` is not linked (with it would
come the arms, room it does not have). Going in takes about 0.75 s, the
dungeon 1.6 s (its generator). The goldens `openworld-town` and
`openworld-dungeon` (and their `-reu` twins) freeze mid-step inside;
`openworld-out` goes into Millford and out, and walks to where its people
would be if they had come too (they once did: nothing emptied `sm_npc_n` on
the way out). It is a text dump, not `--hex`, since only that shows sprites,
as `@` -- which is also how character 0 prints, so compare, do not count.
**A town is its own size**, up to `TOWN_MAX_W` by `TOWN_MAX_H` (40 by 25):
`assets/towns.txt`'s rows say how wide and their count how tall, and
`tools/mktowns.py` writes the maps with `town_w[]`/`town_h[]` into
`src/lib/town_maps.c`, which only the town overlay links (`ow_town_maps.o`;
the people and names stay in `town_data.c`, which owtown and owtalk link
too). The towns the game makes stay `TGEN_W` by `TGEN_H` (21 by 19,
`tgen.s`). `t_enter()` sets `local_w`/`local_h` from them. The maps are in
the overlay, run-length packed (Millford's 21 by 19 is 141 bytes), so it is
the 1426 bytes free beside the town overlay that four bigger towns spend.
Going past 40 by 25 means growing `region[]` and `rowoff[]` (`town.c`) and
the flood's queue in SHORE, and keeping `64 * h + w <= 2112`, which lets a
map be unpacked into the end of `local[]` and moved up into its rows. The
view is 20 squares by 11 (12 full screen) and always centred on the hero,
so any size scrolls and a small town just shows its off-map ground round
it. Only seven people are shown at once (the first seven not under a
roof, `show_people()`), so a bigger town with more people than that needs
the nearest seven instead.

**`make playtown TOWN=id`** (or `gen`, and `AT=x,y`) play-tests one town:
`tools/mkowsave.py --town` writes a save of the game block with the party
inside it (`area`, `place`, `x`/`y` or `OWG_GATE`), which goes on a copy of
the disk as `ow-save`, so F3 at the title starts there. The way out is to
home, outside that town or Millford's square. In a test, wait for the town
with `--until-peek area=N` or a `--settle`, not `--stage 2`: stage 2 comes
only after a step.

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

`make run PROG=owarena` (disk `dist/owarena.d64`): the open world's fight on
its multicolour metatiles, with the party, the monsters and the shots all
sprites, and a menu to pick any fight with. Two parts, and the split is the
point:

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
  (20 squares by 9) with a tile set of the open world's, read by
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
the last (line 198) turns the background black for the text. An event already due when the next is worked out is done there and
then, with `$D012` left alone: written for a line the raster was about to
reach, the VIC latched it, and the next interrupt came at once in the
next phase, which turned the background black for the text from the
middle of the field until the plan changed. **That one
needs no stable raster because row 18 is solid**: a character with every
pixel set shows no background, so nothing shows of where in it the colour
changes. Do not put text or art with background in that row.

**Text** is in the field's multicolour mode, where a colour of 0-7 makes an
ordinary character: so text has those eight colours only, and its letters
are the ROM's lowercase set copied to characters `$80-$FF` (`mx_font()`;
`t_ch()` turns PETSCII into them). Row 19 names the foe and lists their
health, 20-21 the party (a column of ten each, draughts and arrows in the
fourth), 22-24 the log.

**The rules are the classic game's `battle.c`'s** (tag `v0.11.0`), ported
by hand: the flood, the lone monster's banked turns, `LONE_HP`,
`ARCHER_EYE`, the spells, and the autoplayer. What differs from it: the
field is 20 by 9, not 21 by 19 (so two to four boulders, and long ranges
mean less), and **it is eight ways**, as the open world walks: Q, E, Z and X (or the
stick's diagonals) step and strike diagonally; the flood goes
eight ways, a monster strikes from any of the eight squares round someone
(a big one from twelve places), and neither side squeezes diagonally
between two corners of wall. The monsters' shots fly eight ways too (a
big one shoots diagonally from its corner), and stop where they would pass
between two corners of wall (`pinched()`). **The party's shots are
aimed**: `^` (or the stick's button), or a Spark or Bolt, shows the
cursor (`V_CUR`, solid yellow while `aiming`) on the nearest foe the line
reaches within the range (`choose()`, `targets()`: Bresenham's line,
eight-connected, `trace()`, counted as steps are, and stopped by a block,
two corners of wall, one of the party or another foe, so no shot flies
over the party and none can be aimed through one); the direction keys go
through the others left to right, SPACE, `^` or `*` looses, and any other
key puts it down with no turn spent ("Nothing is in reach." when there is
none). The shot glides straight there in pixels, two frames a step, the
arrow's picture the nearest of its eight (`loose()`). **FIGHTBOT keeps
the eight ways** (`#ifdef FIGHT_BOT`: `which_way()` and the old `loose()`,
where a shot that meets one of the party hits them, at the attack it
would have had, and the autoplayer will not shoot through one), for room,
and because `balance.py`'s figures are for it: the aimed shots are kinder
to the party than those, which a sweep has not yet measured. **Ground is slow**: each square's flag has a cost in
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
ground where it is quicker. **What those cost** (`balance.py`, 40
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
mirroring; `$5700-$57FF` (four blocks) are the rounded corners
(`src/fight/corners.s`, `SPR_CORNER`: "The rounded viewport"). `owarena-arrow-ul` in
`tools/test.sh` checks the block after Alaric's arrow at Maldrath, up and
to his left, golden `owarena-dragon` shoots and casts a spark into
Maldrath, and golden `owarena-shore` has nothing in an archer's reach. **`tools/balance.py`** measures
this fight: `owarena`'s `*` loads `FIGHTBOT` (the module built with
`-DFIGHT_BOT`: `battle.c`'s autoplayer, eight ways, which waits for no
frame) and fights the poked fight `auto_runs` times, counting into `res_*`
and the block's `rounds`, then probe stage 5. `--disk` points it at another
copy of the arena's disk, so variants of the module can run side by side.
`ARCHER_EYE` and `WIZ_GUARD` are `#ifndef` for such a sweep (`-D`).
**Measured against the game's own fight** (40 runs a fight, the keepers,
the classic game's `balance.py` and this one): eight ways, a lone warrior, W+Z and
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
and the fight's alike (`?` in a fight lists the fight's over the text rows,
`help()`): Q E Z X diagonally (C does nothing), `M` the music,
SHIFT+`M` the effects (the fight's `next_key()` folds every other shifted
letter onto its plain one), and in a fight the up-arrow key (`$5E`, the
stick's button) aims and shoots and `*` casts -- a wizard's up-arrow casts too, and
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

`actor.h`. Up to three made by `owparty` (`party_make()`), a fourth place
kept for a computer-run companion (not written yet). **Pronouns**: each
member's are the top two bits of their `known` (`ACT_PRON()`, `PRON_*`:
0 they/them, he/him, she/her, it), which the six spells leave free, so
neither the game block nor the fight's grew; `party_make()` clears them, so
set them after it. The save's version is `OWG_MAGIC` (`0x48` since then,
`0x49` since the treasure maps): the bootstrap's `save_version()` takes a
save of the one before (`OWG_MAGIC_V1`) as the same block, everyone
they/them. A monster's are
the top two bits of its bestiary `kind` (`MP_*`, battle.h; `(kind >> 6) ^
3` is a `PRON_*`, so left out is "it"): Maldrath, the Drowned King and the
Gaoler he, the bog hag she, and the people (cutthroats, bandits, centaurs,
efreets, storm giants) `MP_PERSON`, rolled he, she or they for each one in
a fight from its seed (not `rand()`, so the dice are as they were), which
`fight.c`'s lines use (`foe_pron[]`, `say_did()`: "She falls.", "They
fall."). Nothing the game says used a member's "they", so their pronouns
show on the party screens (`I`'s `P` changes them) and in owparty's own
questions ("Send her home"). **`FIGHTBOT`** (the fight module with the autoplayer) had 20
bytes free under `$C000` after this: a fight.c change may need room found
(the panel's health in three colours did not fit, so FIGHTBOT keeps the
old two: `#ifndef FIGHT_BOT` in `show_party()`). The gold, draughts,
arrows, `party_level` and `party_xp` are shared; health, arms (`weapon`,
`armour`, `bow`, indices into the lists in `arms.c`) and a wizard's magic and
spells (`known`/`ready` bits) are each member's. What a class may carry is
`gear_allowed()`, and "better" is by attack or defence, never by index: the
staff is after Dragonbane in the weapon list and the robes and hunting bow
were put in at index 1, so the shops' tier tables are offset for mail and
bows. Whatever a shop sells is `offer()`ed in `shop.c`, which asks who takes
it when more than one would be better for it. Home's generated town always
has the magic shop as its third building (`tgen.s`, and `townref.py` with
it), which sells robes and the books of Heal, Ward and Sleep; the only
other magic shop is Ashcombe's (`assets/towns.txt`), which sells robes and
Ward, Sleep, Bolt and Blaze (`book0` in `shop.c`'s `stock()`, by `land`:
any magic shop away from home sells the second range).

In a fight each living member takes a turn, then the monsters: the flood
fill starts from every member at once, so a monster goes for whoever it can
reach soonest. A member at 0 health has fallen; `get_up()` stands them up on
1 after a fight won or fled. **The balance**, all at the top of
`src/fight/fight.c`: a monster that comes alone gets `LONE_HP` quarters more
health and `LONE_ATK` more attack for each member past the first, and takes
(n + 1) / 2 turns a round, banked between the members' turns (`lone_bank`);
an archer's arrows get `ARCHER_EYE`; a wizard's wards turn `WIZ_GUARD` of
each blow, their spells grow with the level, and they get a point of magic
back each round. A lone warrior was the original tuning (two or three
draughts a keeper, five or six for Maldrath); a party should be near it.
Measure again (`tools/balance.py`, about two minutes a fight at 40 runs:
"The multicolour arena") after touching any of it, or a monster, or the
arms. Three wizards and three archers come out harder than a mixed party,
by design: they are the extremes.

## Testing

**Do not run the full suite (`make test`) locally.** Push a branch and let
the GitHub Actions workflow (`.github/workflows/test.yml`, which runs on every
push to any branch) run it, and watch that result instead. `tools/test.sh
NAME...` runs a few units, which is fine.

**A cloud session can run single tests itself.** `sudo tools/cloud-setup.sh`
first: it installs CI's cc65 and headless VICE 3.10 from
`wesbiggs/c64-cloud-toolchain`'s release (no token needed), or, if that
download is blocked, Ubuntu's VICE 3.7.1 with the ROMs taken from VICE
3.10's source tarball and an `x64sc` wrapper that runs it on Xvfb
(DEVELOPERS.md has the details). Then
`tools/test.sh NAME` checks one case in seconds instead of a CI round. On
3.7.1, CI's result is still the one that counts.

Call `probe_stage(n)` at points worth asserting on. Goldens live in
`tests/golden/<name>.txt` and cover the whole 40x25 screen, so any layout
change shows up as a diff. Re-bless deliberately, never reflexively.

**cc65's `rand()` starts from a fixed seed**, the same in every program, so
without help every visit hears the same rumour and every fight goes the
same way. Sampling a timer at startup is **not** a fix, though it passes a
casual glance: `srand(CIA1_TIMER)` at a fixed point after boot gave the same
seed two runs in four when measured, because the machine is doing the same
thing at the same cycle on every boot. What is genuinely unrepeatable is how
long the player takes to press a key: the splash's seed is the frames the
player took, `owtown` seeds from the game's turn and seed, and the arena
folds its waits into `rng_stir()`/`reseed()` (`arms.c`). `dungeon_build()`
seeds deliberately, from the dungeon, the level and the game's `lore_seed`
-- the same level on every visit, different dungeons in the next game.

**Measure before optimising on this machine; the intuitions do not
transfer.** The classic game's field of view cleared the squares lit last
turn, and the obvious 6502 move -- walk the array with a pointer instead of
an index -- measured *slower* than the indexed loop it replaced (27.1ms
against 18.5ms), because cc65 keeps the pointer out of zero page. What
actually helped was not touching all 399 squares at all: keeping a list of
what was lit took it to 4.4ms. `--peek` plus the jiffy clock at `$A2` over
64 iterations is enough resolution to settle questions like this in a couple
of minutes.

**Disk access needs MEMSIZ raised first.** `$0283` points at the top of
BASIC memory, which is `$A000` on a stock machine — below this game's BSS.
Leave it there and KERNAL SAVE writes nothing and LOAD walks into the weeds,
both without an error you can see. The bootstrap, `ow_run.s`'s loader and
`talk.c` raise it to `$CF00` before touching the drive.

**`cbm_save`'s last argument is a byte count, not an end address** — unlike
almost every other save routine on this machine. Passing an end address
produces a two-byte file containing only the load address, and a silent one:
the call returns success.

**A test that walks the overworld meets nothing** only because the quick
start's game block has `OWG_CALM`: the rolls come from the game's turn, so a
change to the walk changes them. `--poke enc_always=1` makes every roll a
fight, for a test that wants one.

**People block movement**, so two NPCs in a line make the inner one
unreachable. `tools/checkmap.py` checks this; run it after editing
`assets/towns.txt`.

## Testing: the harness

`tools/screendump.py --keys "..."` types into the KERNAL's keyboard buffer
(`$0277`/`$C6`) directly — VICE's own key injection proved unreliable — which
is how to drive the game without a human.
`--shot` renders a PNG from emulated memory (screen RAM + colour RAM + the
charset), so it can capture a screen mid-conversation; VICE's own
`-exitscreenshot` only fires on quit.

**Do not measure anything mid-fight from the probe page.** The probe is
published between turns, so a fight still in progress never reaches it (the
classic game read an encounter rate of about 8 percent as a flat zero that
way). `--peek` a counter directly instead.

**The hero is a sprite, not a screen code**, so screen RAM holds the
terrain under them. `screendump.py` puts `@` wherever a sprite sits in its
text dump (a multicolour one by the middle of its square, which its bob does
not move; the engine's people from the multiplexer's plan, the fight's with
`--mux`), which is how the goldens record where everyone stands; `--raw` and
`--hex` show plain screen RAM. `--shot` draws sprites too.

**Header dependencies are tracked** through cc65's `--create-full-dep`
(`build/**/*.d`), so regenerating `include/q4k/*.h` -- a new tile moving
every actor's code, say -- rebuilds whatever includes it. Before this,
stale objects quietly used the old values: new furniture drew in the
keeper's red, and villagers once drew as drawbridges. A build dir from
before the change has no `.d` files, so clean it once. The `HIMODS` caveat
under "The library's segments" still stands: flags are not dependencies.

`--peek SYM[+OFF][:WIDTH]` (or `$ADDR` for an address) prints any global after the run (wider than 8
bytes, just the bytes as hex, last first); `--poke
SYM=VALUE` (and `SYM+OFFSET=VALUE`) writes one before it. Both use the label
file ld65 emits next to the binary. Prefer it over adding debug
hooks to the game: the quest, the inventory and the party struct are all
reachable from a test this way, and none of it ships.

**The open world needs its disk**: its tests start the bootstrap with
`--disk dist/openworld.d81 --splash quick` (see "The open world"), and
`make` rebuilds the disk after any change. Never `git stash` round a
`make`: the objects built from the stashed tree are newer than the sources
that come back, and stay.

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

**`tools/test.sh` runs units side by side**: every golden and every case
of its own is a unit (`--list`), run `-j N` at a time (default the CPUs),
each printing PASS or FAIL with its seconds when it ends, then the slowest
eight. `tools/test.sh NAME...` runs just those; `--group
goldens|cases` a half, which is how CI splits the suite over two machines
(`group_of()`); `make test TESTARGS=...` passes them on. A new case is a `u_` function, a line in
`run_unit()` and its name in `all_units()`, slowest first. **`--settle` is
wall-clock seconds, warp or not**, and was most of the suite's time (about
13 of CI's 21 minutes), so it ends early once what the case checks for has
held on two looks a second apart (`settle()` in `screendump.py`):
`--expect GOLDEN` (which `compare()` adds, except when blessing, which
wants the end of the settle), `--until REGEX` on the dump, `--until-peek
SYM=VALUE`. Give a new case one of them, the same as what it checks, and
its `--settle` becomes only how long it may take; a case that fails still
waits all of it. Don't edit `screendump.py` while a
run is going: the cases that start afterwards load the half-edited file.
