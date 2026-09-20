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

Generated C (`src/lib/*_data.c`, `include/q4k/tiles.h`, `world_data.h`,
`town_data.h`) is checked in so the build needs no Python. `tools/mktiles.py`,
`mkmap.py` and `mktowns.py` regenerate it; `tools/genworld.py` drafted the
overworld once and is not part of the build.

## Playing

`WASD` or the cursor keys move. Walk into someone to talk to them or open
their shop. `T` talks to whoever is beside you, `SPACE` waits, `Q` quits.
In a fight: `A`ttack, `D`efend, `P`otion, `R`un.

## Workflow

```
make                       build every program
make run PROG=hello        play it in VICE
make play                  play the game with NPC dialogue attached
make check PROG=hello      headless: print the C64 screen as text
make shot PROG=hello       headless: save build/hello.png
make test                  headless: diff every program against its golden
make bless                 accept current output as the new goldens
make d64 PROG=hello        build a disk image
```

`make check` is the fast inner loop. It boots the program in a headless VICE,
waits for a handshake, and prints the 40x25 screen:

```
  0 |HELLO, WORLD                            |
  ...
  8 |               #########                |
  9 |               #..██...#                |
 10 |               #..██..T#                |
 11 |               #...@..T#                |
```

Add `--hex` for raw screen codes, `--color` for colour RAM, `--keys "wasd"`
to feed the keyboard, `--stage N` to wait for a later checkpoint, `--shot` to
render a PNG from emulated memory.

`--poke quest_stage=7 --poke items=0x38` sets any global by name, resolved
from ld65's label file. That is how a test reaches the middle of a long quest
without walking there, and it keeps cheat hooks out of the game itself.

## How the headless harness works

The program writes progress markers to the **probe page** at `$CF00` (see
[include/q4k/probe.h](include/q4k/probe.h)); `cfg/c64-q4k.cfg` lowers
`__HIMEM__` to `$CF00` so the C runtime can never clobber it.
`tools/screendump.py` drives VICE's binary monitor over TCP, polls that page
until the program reports the stage it wants, then reads screen and colour RAM
out of emulated memory.

Polling a handshake rather than sleeping means tests run at whatever speed the
emulator manages (currently under a second each) and never race a half-drawn
screen.

## The map

The overworld is a wrapping 64x64 grid; towns and dungeon levels are exactly
one 21x19 viewport, so they never scroll. Towns are authored in
[assets/towns.txt](assets/towns.txt); dungeon levels are generated, seeded
from the dungeon and depth so a level you walk back into is the level you
left.

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
