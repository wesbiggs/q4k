# Quest for the King: developer notes

How the game is built, tested and put together. [README.md](README.md) is
the story and how to play; [CLAUDE.md](CLAUDE.md) is the working notes --
the traps this machine sets, and what has already been learned the hard way.

## Setup

```
brew install cc65 vice
```

That is the whole toolchain: `cc65`/`ca65`/`ld65`/`ar65` build, `x64sc` runs,
`c1541` and `petcat` handle disk images.

On Linux -- a Claude Code cloud session, or anything else on Ubuntu 24.04 --
[tools/cloud-setup.sh](tools/cloud-setup.sh) installs the same: cc65 from
the distribution, and a headless VICE 3.10 either downloaded from the
`toolchain-vice3.10` release (given a token that can read this repo) or built
from source. The `toolchain` workflow builds and publishes that release.

## Make targets

```
make                  build every program into dist/
make run PROG=name    launch dist/name.prg in VICE (default: hello)
make play             the game, with the dialogue daemon on the modem port
make check PROG=name  headless run, dump the screen as text
make shot PROG=name   headless run, save a PNG screenshot
make test             every screen test against its golden, plus checkmap
make bless            accept current output as the new goldens
make assets           regenerate the C and JSON built from assets/
make d64 PROG=name    a bootable disk image
make release          the release's .prg, .d64 and zip, in dist/release/
make clean
```

`make bless` is for after you have read the diff, never instead of it.

Players get the game from a release and never need any of this; to play
from a checkout instead, `make play` is the same thing as the release zip's
`tools/play.sh`, with the game built from the tree.

## Testing

Every test is a screen: `tests/golden/<name>.txt` is the whole 40x25 text
screen a program should leave, and a `<name>.cmd` beside it, if there is
one, holds the `tools/screendump.py` arguments that get it there -- keys to
type, globals to poke. `tools/test.sh` (which `make test` runs) diffs each
against a fresh headless run, and also runs `tools/checkmap.py` and the
dialogue host's own tests. Save and load get a fresh disk each run.

The `test` workflow (`.github/workflows/test.yml`) runs `make test` on every
push to any branch, and that is the run to watch: the suite takes a while
locally and opens VICE windows on a desktop. `make check PROG=name` or
`tools/screendump.py dist/name.prg --keys ...` is the quick way to look at
one screen while working.

CLAUDE.md covers what the harness can do (`--keys`, `--poke`, `--peek`,
`--shot`, the modem) and the ways a screen test goes flaky.

## Releases

The version lives in `VERSION` and nowhere else: `make` generates
`include/q4k/version.h` from it, and the title screen shows it in the corner.
Releases are tagged `v<version>`, and a tag push runs
`.github/workflows/release.yml`, which publishes:

- `q4k-<version>.prg`, the game;
- `q4k-<version>.d64`, the game on a disk;
- `q4k-<version>.zip`, the game plus the dialogue host and the assets it
  reads, laid out as in the repo so `tools/play.sh` runs from inside it.

`make release` builds the same three into `dist/release/`, which is the way
to look at them before tagging.

To cut one:

1. Set the new version in `VERSION` and run `make`.
2. Re-bless the title screen, whose golden shows the version -- read the
   diff, which should be that one line.
3. Add a `## <version>` section to `CHANGELOG.md`. It becomes the release's
   notes, and the workflow refuses to publish without one.
4. Merge to `main`, then tag it and push the tag:
   `git tag v0.9.0 && git push origin v0.9.0`.

The workflow refuses a tag that does not match `VERSION`, and checks the
built title screen against its golden before publishing. The test workflow
runs on the same tag push. A release already drafted by hand for the tag
keeps its text and just gets the files.

The `toolchain-vice3.10` release holds the headless VICE for
`tools/cloud-setup.sh`. It is marked a pre-release so that it is never the
one GitHub calls "Latest", which is the link the README gives players.

## Layout

```
src/<name>.c        a program with main() -> dist/<name>.prg
src/lib/            shared modules, archived into build/q4k.lib
include/q4k/        public headers
cfg/c64-q4k.cfg     the game's memory map; c64-tool.cfg for the small tools
assets/             tiles, overworld, towns, world canon, NPC roster
host/               daemons that answer the C64's modem
tools/              build, run, screenshot, screen dump, tests
tests/              expected screen output
VERSION             the version, and the only place it is written
CHANGELOG.md        what each release changed; its notes on GitHub
```

Besides the game, `make` builds five dev tools: `tiles` (the whole tile set),
`charset` (all 256 screen codes), `dungeontest` (preview generated dungeon
levels), `arena` (fight any monster on any ground, at any level and in any
kit) and `serialtest` (a modem loopback).

Generated files (`src/lib/*_data.c`, `include/q4k/tiles.h`, `world_data.h`,
`town_data.h`, `version.h`, `assets/geography.json`) are checked in so the build needs no
Python, and `make` regenerates any whose assets have changed.
`tools/genworld.py` drafted the overworld once and is not part of the build.

## The arena

`dist/arena.prg` is a fight with anything, on any ground, without walking
the overworld until the right thing turns up. It runs `battle.c`'s own
fight -- `battle_fight()` -- not a copy, so what you see there is what the
game does.

```bash
make run PROG=arena
```

The menu lists every monster: the wanderers in level order, then the three
keepers, then Maldrath.

| key | |
|---|---|
| `a`-`o` | fight that monster |
| `1`-`6` | the ground a wanderer is met on: grass, forest, shore, swamp, lair, floor |
| `F1` | hero level, 1 to 10, with the matching full health |
| `F3` / `F5` / `F7` | weapon / armour / bow, cycling through what the shops sell |

Keepers are always met in a dungeon room and Maldrath in his lair, as in the
game; wanderers can be run from and they cannot. The hero is healed and
given three draughts before every fight, and whatever the fight changed --
experience, gold, a level gained -- is put back afterwards, so the settings
on the panel are what the next fight gets. Random numbers are stirred by how
long you take to press a key, as in the game, so the same choice does not
play out the same way twice.

The menu is a golden (`tests/golden/arena.txt`), so a monster added, renamed
or recoloured shows up in `make test`. A fight is not: the monster's
starting square is random, so no two runs agree.

It also drives headlessly, which is how to check a change from a script:

```bash
tools/screendump.py dist/arena.prg --keys 'd' --settle 3          # a wasp, on grass
tools/screendump.py dist/arena.prg --keys '\x85\x852h     ' --settle 5 \
    --shot build/archer.png                                       # level 3, forest, archer
```

`F1` to `F7` are `\x85`-`\x88` in `--keys`, and each space is a turn spent
standing your ground.

## Monsters

Everything that fights lives in `src/lib/battle.c`, typed in
`include/q4k/battle.h`:

- `wanderers[]` -- each with `from`, the level it starts turning up at, and
  kept in that order: `battle_wander()` picks evenly from those the hero has
  reached and stops at the first it has not.
- `keepers[]`, one per dungeon, and `maldrath`, who is two squares by two.

A `Monster` is its stats, its glyph and colour, its speed in tenths of a move
a turn, its size, and optionally a ranged attack: `shot` (strength), `reach`
(squares) and `kind` (`SHOT_ARROW`, `SHOT_FIRE` or `SHOT_WATER`, which pick
the shot sprite's shape and colour and the line in the log). Leave those out
and it only closes in. A ranged monster shoots when the hero is in a
straight line, within reach, with nothing solid between, and strikes like
anything else when beside them.

A new monster usually wants a glyph: add a `tile ... actor <colour>` to
`assets/tiles.txt` at the next free code (see CLAUDE.md for what is taken),
and `make` regenerates `T_<NAME>` and `TC_<NAME>`. Then try it in the arena.

A fight starts with the monster anywhere in the top third of the field that
its whole body can stand on and that has a way through to the hero. The
check is the monster's own pathfinding flood, so no fight starts with it
shut in a pocket of trees; after 16 misses it falls back to the field's
spot in `assets/arenas.txt`.

## Balance notes

A healing draught restores a share of full health rather than a fixed
number. A flat amount was most of a health bar at level one and less than
one of Maldrath's blows at level ten, which made drinking during the fight
that needs it a way to stand still. The price climbs to match, five marks
plus five a level (`potion_price()` in `src/lib/shop.c`): the dozen or so
needed for the dragon come to about what the best armour costs, so the last
shopping trip is a decision rather than small change.

## Saved games

Saves go to a disk in unit 8 (`src/lib/savegame.c`). Only the character,
the quest's progress and which squares have been explored are written:
towns are fixed and dungeon levels regenerate exactly from their seed, so
the map is rebuilt rather than stored. CLAUDE.md has the two traps disk
access sets on this machine.

## The map

The overworld is a wrapping 64x64 grid; towns are exactly one 21x19
viewport, so they never scroll. Dungeon levels are `DUN_W` x `DUN_H` (48x32,
in `include/q4k/game.h`) and the view follows the hero, clamped at the
edges. Towns are authored in [assets/towns.txt](assets/towns.txt); dungeon
levels are generated, seeded from the dungeon and depth so a level you walk
back into is the level you left.

Both share `local[]`, whose rows are `LOCAL_STRIDE` (64) apart whatever the
place's width, so a square is `LOCAL_AT(x, y)`: a shift, not a multiply.
It has no storage of its own -- it is the 2K font image, which is dead once
`charset_install()` has copied it to `$3800` (see `src/lib/local.s`). So the
charset must be installed before anything writes a map, which is why
`main()` calls `tiles_install()` before `game_init()`.
`make shot PROG=dungeontest` draws a level at half scale, a character to
every 2x2 block of squares, since a whole one no longer fits on screen.

Dungeons are dark. A room lights up whole when you stand in it, you can see
straight along a corridor until something stops the eye, and squares you
have already seen stay on the map drawn dim. That is Rogue's model rather
than a full symmetric shadowcast, and it is the right one here: the
generator only cuts corridors along the axes, so four rays capture exactly
what a corridor shows, for a fraction of the cost of casting at every wall.
Measured on the emulated machine it costs 4.4ms a turn against a 34ms
redraw.

What you have seen is kept for every level of the last dungeon you were
in, a bit per square. `fov_enter()` in `src/lib/fov.c` files the level you
left away when you arrive on the next one, not when you leave, so dying
down there needs no code of its own: nothing touches the map between the
fight and walking back in. A saved game keeps only the level it was saved
on; the other levels would cost the save blob 576 bytes of BSS it does not
have.

`tools/checkmap.py` (run by `make test`) floods the overworld from the start
square and each town from its arrival square, and fails if any town, dungeon,
lair or shopkeeper cannot be walked to. It has already caught two bugs a
screen test never would: the dragon's lair walled inside the mountains, and a
shopkeeper standing in a doorway blocking the smith behind him.

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

Mixed case needs no special handling. cc65 encodes an upper-case letter in a
string literal as shifted PETSCII, which draws as a capital, and a lower-case
one as unshifted, which draws in lower case — the charset holds both, and
no tile takes a letter's slot: terrain is in the graphics range at
`$60-$7D`, and the people and monsters are in the reverse-video half at
`$80-$90`, which the game never draws text from.

## Picking tile glyphs

`make shot PROG=charset` renders all 256 screen codes so you can choose by eye.
Screen codes are neither ASCII nor PETSCII — they index the active character
set, and q4k selects the lowercase/mixed set (`$D018 = $17`), where `$41-$5A`
are `A-Z` and `$01-$1A` are `a-z`.

## The dialogue host

The game dials out over the C64's **user port** — the modem port — at 300
baud. VICE bridges that to a TCP socket on this machine, where
[host/npcd.py](host/npcd.py) answers: it takes the line the player typed, asks
a local model to answer as the NPC, and sends one line back.

The model is whatever [Ollama](https://ollama.com) is serving on its
OpenAI-compatible endpoint, default `llama3.2:3b`. The daemon uses nothing but
the Python standard library — no packages to install, no API key, no network
egress.

```bash
ollama serve &                 # if it is not already running
ollama pull llama3.2:3b

make play                      # live dialogue
Q4K_MOCK=1 make play           # canned replies, no model needed
Q4K_MODEL=llama3.2:1b make play
```

Model and endpoint are configurable three ways, in increasing precedence: the
constants at the top of [host/npcd.py](host/npcd.py), the `Q4K_MODEL` and
`Q4K_OLLAMA_URL` environment variables, and `--model` / `--url`. Anything that
speaks the OpenAI chat shape works — another Ollama host, llama.cpp's server,
a compatible proxy.

If Ollama is unreachable or the model is not pulled, the daemon says so and
serves canned lines rather than failing -- but it also answers the C64's
startup `PING` with `ERR no model`, and the C64 then plays without
conversations, as it does when nothing answers at all. `--mock` on purpose
answers `OK mock`, and the C64 talks through it; that is what the tests
and `Q4K_MOCK=1` use.

### Directions they can be trusted on

Left to itself a model invents geography — it will send you north to a town
that is south, over a mountain range that is not there.
[tools/mkgeo.py](tools/mkgeo.py) measures the real map instead: compass
bearing on the wrapping grid, and walking distance by flood fill over
passable ground, which is what actually matters when the Sundering runs down
the middle of the continent. The result goes to `assets/geography.json`, and
the daemon puts the lines for wherever the speaker is standing in front of
them, with an instruction not to guess at anything else.

    from Millford: THE HOLLOW CROWN lies south, about 46 leagues on foot,
                   though the road bends a long way round.

**The C64 sends only an NPC id.** Every system prompt lives host-side and is
looked up by that id: [assets/canon.md](assets/canon.md) is the world bible
shared by all of them, and each character's voice and private knowledge come
from [assets/npcs.json](assets/npcs.json). Adding a character is a JSON entry
plus an `ent` line and a digit on the map in
[assets/towns.txt](assets/towns.txt) — no change to the protocol, and
nothing new on the C64.

Protocol, CR-terminated ASCII both ways:

```
->  HI <npc> <stage> <seed>           open a conversation
->  SAY <npc> <stage> <seed> <text>   say something
->  ALE <inn> <stage> <seed>          buy the innkeeper a drink's worth
->  PING                              at startup: is anyone there to talk?
<-  OK <reply>   |   GIVE <g> <reply>   |   BYE <reply>   |   ERR <reason>
```

`<stage>` is one digit of quest progress. `<seed>` is two hex digits rolled
once per game and kept in the save: the host draws that run's secrets from
[assets/secrets.json](assets/secrets.json) with it, so the word Old Brann
gives you for the Stair's dry hour is different every game. `GIVE` is the
host deciding a conversation has earned something -- Vane only hands over
the Tidal Key when you say that word. The model never makes that call;
the host matches the player's line itself, and the model is never even told
the word it is waiting for. `BYE` is the NPC ending the conversation after
that reply -- a goodbye, offence taken, work to get back to. `ALE` is
answered with an authored hint from [assets/hints.json](assets/hints.json)
that fits the inn and the stage; no model is involved. With no host on the
line at all, nothing is gated, so the game stays winnable without the modem.

### Taming a small model

Small local models follow instructions loosely, so the daemon does not trust
the prompt to do the work:

- The prompt puts the binding constraints last, phrased as what to do, and
  insists on first person — without that the fisher talks about Old Brann in
  the third person.
- Replies are cut to two sentences, stripped of stage directions, speaker
  labels and wrapping quotes, forced to printable ASCII, and trimmed to 150
  characters at a sentence boundary.
- An empty reply falls back to an authored line from the NPC file.

What no amount of post-processing fixes is invention. `llama3.2:1b` will
assert things the canon does not say — it put the Greenhollow grain factor,
who sells to the castle, buying grain from a fishing town. `llama3.2:3b` gets
that right and quotes the reeve's four unanswered letters back accurately,
for about 1.3s a reply instead of 0.25s. Since an answer then takes several
seconds to crawl down the line, 3B is the better trade and the default.

### Why 300 baud

The KERNAL's RS-232 is NMI-driven bit-banging, and its receiver drops bytes on
sustained back-to-back traffic well before its nominal top speed. 300 baud is
what it handles reliably — and it is what a 1983 modem ran at anyway, so an
NPC's answer arrives at reading pace.

The test harness drops VICE's warp mode whenever the modem is wired up:
warp decouples emulated time from wall-clock time, and the C64's timeouts
would expire in a fraction of a real second, long before any host could reply.
