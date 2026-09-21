# Quest for the King

A grid-based RPG roguelike for the Commodore 64, in the spirit of Ultima III.
Written in C (cc65) with assembly where it pays, run and tested under VICE.

A letter comes from King Matheson. The dragon Maldrath has been burning
Keldmarch for three winters and two armies have not come back. The blade that
can banish him was broken, and its three binding stones are sealed in three
deep places. Buy a sword, walk south, and start asking people questions.

The plot is deliberately the oldest one there is. What is not old is who you
talk to: every NPC answers in their own voice, from the world's canon and
from how far you have got, through a model running on the host at the other
end of the C64's modem port.

## Setup

```
brew install cc65 vice
```

That is the whole toolchain: `cc65`/`ca65`/`ld65`/`ar65` build, `x64sc` runs,
`c1541` and `petcat` handle disk images.

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
```

Besides the game, `make` builds four dev tools: `tiles` (the whole tile set),
`charset` (all 256 screen codes), `dungeontest` (preview generated dungeon
levels) and `serialtest` (a modem loopback).

Generated files (`src/lib/*_data.c`, `include/q4k/tiles.h`, `world_data.h`,
`town_data.h`, `assets/geography.json`) are checked in so the build needs no
Python, and `make` regenerates any whose assets have changed.
`tools/genworld.py` drafted the overworld once and is not part of the build.

## Playing

The title screen sets the scene and asks your name; RETURN takes the default.
Then `WASD` or the cursor keys move. The panel shows what you are carrying
and wearing, alongside your stats and any quest items. Walk into someone to
talk to them or open their shop.

| key | |
|---|---|
| `T` | talk to whoever is beside you |
| `P` | drink a healing draught |
| `F1` / `F3` | save / restore |
| `SPACE` | wait a turn |
| `?` | keys, and what you are supposed to be doing next |
| `Q` | quit |

In a fight: `A`ttack, `D`efend, `P`otion, `R`un.

A healing draught restores a share of your full health rather than a fixed
number, so it is worth a turn late on as well as early. A flat amount was
most of a health bar at level one and less than one of Maldrath's blows at
level ten, which made drinking during the fight that needs it a way to
stand still.

Saved games go to a disk in unit 8; `make play` creates `dist/saves.d64` the
first time and attaches it. Only your character, your progress and which
squares you have explored are written — towns are fixed and dungeon levels
regenerate exactly from their seed, so the map is rebuilt rather than stored.


## The map

The overworld is a wrapping 64x64 grid; towns and dungeon levels are exactly
one 21x19 viewport, so they never scroll. Towns are authored in
[assets/towns.txt](assets/towns.txt); dungeon levels are generated, seeded
from the dungeon and depth so a level you walk back into is the level you
left.

Dungeons are dark. A room lights up whole when you stand in it, you can see
straight along a corridor until something stops the eye, and squares you
have already seen stay on the map drawn dim. That is Rogue's model rather
than a full symmetric shadowcast, and it is the right one here: the
generator only cuts corridors along the axes, so four rays capture exactly
what a corridor shows, for a fraction of the cost of casting at every wall.
Measured on the emulated machine it costs 4.4ms a turn against a 34ms
redraw.

`tools/checkmap.py` (run by `make test`) floods the overworld from the start
square and each town from its arrival square, and fails if any town, dungeon,
lair or shopkeeper cannot be walked to. It has already caught two bugs a
screen test never would: the dragon's lair walled inside the mountains, and a
shopkeeper standing in a doorway blocking the smith behind him.

## Picking tile glyphs

`make shot PROG=charset` renders all 256 screen codes so you can choose by eye.
Screen codes are neither ASCII nor PETSCII — they index the active character
set, and q4k selects the lowercase/mixed set (`$D018 = $17`), where `$41-$5A`
are `A-Z` and `$01-$1A` are `a-z`.

## Talking to people

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
serves canned lines rather than failing.

### Directions they can be trusted on

Left to itself a model invents geography — it will send you north to a town
that is south, over a mountain range that is not there.
[tools/mkgeo.py](tools/mkgeo.py) measures the real map instead: compass
bearing on the wrapping grid, and walking distance by flood fill over
passable ground, which is what actually matters when the Sundering runs down
the middle of the continent. The result goes to `assets/geography.json`, and
the daemon puts the lines for wherever the speaker is standing in front of
them, with an instruction not to guess at anything else.

    from Millford: CASTLE MATHESON lies south, about 64 leagues on foot,
                   though the road bends a long way round.

**The C64 sends only an NPC id.** Every system prompt lives host-side and is
looked up by that id: [assets/canon.md](assets/canon.md) is the world bible
shared by all of them, and each character's voice and private knowledge come
from [assets/npcs.json](assets/npcs.json). Adding a character is a JSON entry
plus a coordinate in the `places` table in [src/q4k.c](src/q4k.c) — no change
to the protocol, and nothing new on the C64.

Walk onto a town or the castle and press **T**.

Protocol, CR-terminated ASCII both ways:

```
->  HI <npc>              open a conversation
->  SAY <npc> <text>      say something
<-  OK <reply>   |   ERR <reason>
```

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
