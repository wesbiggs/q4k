# Quest for the King: developer notes

How the game -- the open world -- is built, tested and put together. [README.md](README.md) is
the story and how to play; [CLAUDE.md](CLAUDE.md) is the working notes --
the traps this machine sets, and what has already been learned the hard way.

## Setup

```
brew install cc65 vice
```

That is the whole toolchain: `cc65`/`ca65`/`ld65`/`ar65` build, `x64sc` runs,
`c1541` and `petcat` handle disk images.

On Linux -- a Claude Code cloud session, or anything else on Ubuntu 24.04 --
run `sudo tools/cloud-setup.sh` (or paste it into the environment's setup
script). It runs [c64-cloud-toolchain](https://github.com/wesbiggs/c64-cloud-toolchain)'s
`setup.sh` at the release CI pins (`TOOLCHAIN`), with `python3-pil` for
`screendump.py --shot`. That release's prebuilt cc65 2.19 and headless VICE
3.10, in `/opt/c64-toolchain` and linked into `/usr/local/bin`, are exactly
what CI runs (`.github/workflows/test.yml` uses the same repository as an
action, at the same tag: move the two together). If GitHub's release
downloads are blocked, it falls back to Ubuntu's cc65 and VICE 3.7.1, with
the ROMs from VICE's source and an `x64sc` wrapper that starts Xvfb; its
README has the details. CI's 3.10 is the reference for the screen tests: if
3.7.1 ever disagrees, CI is right. `mkfont.py` finds the character ROM
beside whichever `x64sc` is on the `PATH`.

To try a change to the toolchain, check it out beside this repository
(`../c64-cloud-toolchain`): `tools/cloud-setup.sh` runs that copy instead.

## Make targets

```
make                  build everything: dist/openworld.d81, dist/owarena.d64
make run              the open world in VICE, from its disk
make run PROG=owarena the multicolour arena, from its disk
make playtown TOWN=id play-test one town of assets/towns.txt (or TOWN=gen,
                      the one the game makes); AT=x,y to start on a square
make test             every case against its golden, and the checks
make bless            accept current output as the new goldens
make assets           regenerate the C and assembly built from assets/
make clean
```

`make bless` is for after you have read the diff, never instead of it.

`make playtown` builds what changed (a town's map, say), copies the disk to
`build/playtown.d81` and writes it a saved game with a lone warrior inside
that town, at the map's `$` or at `AT`'s square, holding every key and
stone, with nothing wandering out. Press **F3** at the title to load it.
Leaving the town puts him on Keldmarch outside it (outside Millford, for a
town across the sea), and F1 saves to the copy, not to
`dist/openworld.d81`.

## Testing

Most tests are a screen: `tests/golden/<name>.txt` is the whole 40x25 text
screen a run should leave, and the `<name>.cmd` beside it holds the
`tools/screendump.py` arguments that get it there -- the disk, keys to type,
globals to poke. `tools/test.sh` (which `make test` runs) diffs each against
a fresh headless run, and runs the cases of its own: the save, a fight, a
dungeon's monsters, the REU cache, the roads (`tools/roadref.py`),
`tools/checkmap.py` (the towns: nobody walled in) and `tools/checkisle.py`
(the generated lands: nothing cut off, and the C64's own `isletest` against
the model).

The cases run several at a time (`-j`), each printing its time when it
ends; `tools/test.sh NAME...` runs only those (`--list` names them). The
`test` workflow (`.github/workflows/test.yml`) runs `make test` on every
push to any branch, split over two machines (`--group goldens`, `cases`),
and that is the run to watch: the whole suite takes a while locally.
`tools/test.sh NAME` is the quick way to check one case while working.

CLAUDE.md covers what the harness can do (`--keys`, `--poke`, `--peek`,
`--shot`, `--splash`, `--reucache`) and the ways a screen test goes flaky.

## Layout

```
src/openworld.c     the bootstrap: the splash, the lands, then an engine
src/ow/             the engine (built twice, ow-cpu and ow-reu) and its overlays
src/owtown.c        the screens of text: shops, inns, the party, help, a fight
src/owtalk.c        a conversation
src/owparty.c       the party's making, for a new game
src/fight/          the fight module, loaded by owtown and by the arena
src/owarena.c       the multicolour arena: pick any fight
src/isletest.c      the generators' check, which tools/checkisle.py drives
src/lib/            shared modules, archived into build/q4k.lib
src/sid/            the theme's player
include/q4k/        headers
cfg/                each program's memory map
assets/             tiles, towns, monsters, fields, music, what people say
tools/              build, run, screen dump, tests
tests/golden/       expected screens
docs/memmap.html    the open world's memory map, drawn by make
```

Generated files (`src/lib/*_data.c`, `src/lib/ow_*.s` from the tile sets,
`include/q4k/tiles.h`, `town_data.h` and the rest) are checked in, and
`make` regenerates any whose assets have changed.

Every hand-written source starts with an `SPDX-License-Identifier:
GPL-3.0-or-later` line (after a `#!`); give a new one the same. Generated
files carry none, and `assets/` is CC BY-SA 4.0 as a whole (README.md's
Licence). (There is no map file:
the lands are made by the bootstrap, from the game's seed.)

## The arena

`dist/owarena.prg` (on `dist/owarena.d64`) is a fight with anything, on any
ground, without walking the world until the right thing turns up. It loads
the same fight module the open world does, so what you see there is what
the game does.

```bash
make run PROG=owarena
```

| key | |
|---|---|
| `a`-`o` | fight that monster |
| `1`-`6` | the ground a wanderer is met on; keepers are always in a dungeon room and Maldrath in his lair |
| `+` / `-` | how many wanderers come at once / how many of the party |
| `F1` | the party's level, 1 to 10 |
| `F3` / `F5` / `F7` | weapon / armour / bow |

`*` has it play by itself, with the module's autoplayer (`FIGHTBOT`), which is
how [tools/balance.py](tools/balance.py) measures the keepers and the
wanderers against each kind of party: dozens of fights, four VICEs at a time.
The menu and three fights are goldens (`owarena*`).

## Monsters

Everything that fights lives in `src/lib/bestiary.c`, typed in
`include/q4k/battle.h`:

- `wanderers[]` -- each with `from`, the level it starts turning up at, and
  kept in that order: `battle_wander()` (`src/lib/ow_fight.c`) picks evenly
  from those the hero has reached and stops at the first it has not. The
  dungeons show them on the map, and keep their own copy of what that needs: each one's
  picture, colour and speed in `src/ow/foes.s` (natives too), and `from` in
  `src/ow/dungeon.c`. Change a wanderer and change those.
- `keepers[]`, one per dungeon, and `maldrath`, who is two squares by two.

A `Monster` is its stats, its glyph and colour, its speed in tenths of a move
a turn, its size, and optionally a ranged attack: `shot` (strength), `reach`
(squares) and `kind` (`SHOT_ARROW`, `SHOT_FIRE`, `SHOT_WATER` or
`SHOT_BOLT`). Leave those out and it only closes in. Its pictures are
`assets/monsters.txt`'s, which `tools/mkfields.py` makes into the fight's
sprite files (and the dungeon's, `src/ow/foe_art.s`). Then try it in the
arena.

## Balance notes

A healing draught restores a share of full health rather than a fixed
number. A flat amount was most of a health bar at level one and less than
one of Maldrath's blows at level ten, which made drinking during the fight
that needs it a way to stand still. The price climbs to match, five marks
plus five a level (`potion_price()` in `src/lib/shop.c`): the dozen or so
needed for the dragon come to about what the best armour costs, so the last
shopping trip is a decision rather than small change.

## Saved games

F1 in the engine saves the game block (`include/q4k/owgame.h`, 193 bytes) to
`OW-SAVE` on the disk, and F3 restores it through the bootstrap, which builds
that game's world first. The disk comes with a save of its own
(`tools/mkowsave.py`). Nothing of the map is stored: the lands are rebuilt
from the seed. CLAUDE.md has the two traps disk access sets on this machine.

## What people say

[src/lib/talk.c](src/lib/talk.c) loads what each person can say from the
disk when you walk up to them, and the player picks a topic from a menu. The
lines are [assets/talk.txt](assets/talk.txt), by quest stage; a line marked
`give` is the one that hands over what the quest needs, and leaving without
hearing it gets you nothing. The inns' rumours are
[assets/hints.json](assets/hints.json), loaded when you buy an ale.
[tools/mktalk.py](tools/mktalk.py) packs one file per person (`0` to `8`, by
`NPC_*`) and per inn (`9` to `11`) onto the disk, and checks that each fits
the talk's buffer (`TALK_SIZE` in the Makefile, 3052 bytes). People's names
come from [assets/npcs.json](assets/npcs.json), and where they stand from
[assets/towns.txt](assets/towns.txt).

The game used to have an experimental networked build, whose people answered
through a language model on a host at the other end of the modem port. It
and its host (`host/npcd.py`) were retired with the classic game; the 0.11.0
tag has them.

## The letters

The game draws from its own copy of the character set in RAM -- the same copy
the tiles are patched into -- so the font is ours to choose.
[tools/mkfont.py](tools/mkfont.py) builds that 2K image from either source:

```bash
make assets                                   # the default: ROM, thickened
make assets FONT_STYLE=serif                  # feet on the stems only
make assets FONT_STYLE=none                   # the ROM as Commodore drew it
make assets FONT_SOURCE=assets/fonts/x.bin    # any C64 charset binary
```

No font binary is checked in. `assets/fonts/*.bin` is ignored, because a
character set is somebody's typeface and most of the good ones are not ours
to redistribute — [assets/fonts/NOTICE.md](assets/fonts/NOTICE.md) has the
detail, and how to drop one in locally.

The `rom` styles exist because eight by eight is too small for blackletter
produced by transformation: the strokes collide and it stops being readable.
Weight and a foot on the stems is about the limit of what that approach
buys. A face actually drawn at this size does far better, which is the whole
argument for taking a `.bin`.

`KEEP_AS_IS` in [tools/mkfont.py](tools/mkfont.py) lists glyphs the
transforms spoil, taken from the ROM unchanged: `M N W` and their lower
cases. All three are defined by narrow gaps between diagonals, and
thickening a stroke rightwards closes them — `W` becomes a solid band, `M`'s
inner V fills until three rows are identical bars. They come out a shade
lighter than their neighbours, which is a better trade than illegible.

The same letters are also a proportional GEOS font, `assets/q4k-serif.cvt`
("Q4K Serif", 8 points, in Convert format), which `make assets` writes from
`src/lib/font_data.c` with [tools/mkcvt.py](tools/mkcvt.py). It is for
vic2orious's `.v2t` text, such as the splash screen's:
`build --font assets/q4k-serif.cvt`, and `font.use()` with the same path for
the kit's `lib/font.inc`. Each glyph is trimmed to its inked columns plus one
of spacing (a space is four wide), as vic2's own ROM font is; the ASCII codes
the C64's set has no glyph for (`\ ^ _ ` { | } ~`) are spaces. Its info block
is complete, so GEOS can use it too.

The screens of text (owtown, owtalk, owparty) draw from this set. The
engine's map and the fight have character sets of their own, made from the
tile sets, with the ROM's lowercase letters copied into their top half.
