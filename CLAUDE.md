# Quest for the King — working notes

Grid-based RPG roguelike for the Commodore 64 (Ultima III lineage).
cc65 + VICE on macOS. See DEVELOPERS.md for the workflow and README.md for
the story and how to play.

## Conventions

- C for game logic; assembly only where profiling says it matters.
- `--static-locals` is on: **no recursion**. Locals are static storage, which
  is much faster than a 6502 software stack.
- Shared code goes in `src/lib/` and is archived with `ar65`, so the linker
  pulls in only what each program references. Adding a module to `src/lib/`
  costs nothing in programs that don't call it.
- Anything with `main()` goes directly in `src/` and becomes its own `.prg`.

## Memory

Three regions, set up in `cfg/c64-q4k.cfg`, and the split matters:

- `$0801-$37FF` **MAIN** -- code and rodata. Twelve kilobytes, and the game
  does not fit in it.
- `$3800-$3FFF` **charset**, built at runtime. Must be in VIC bank 0 and not
  at `$1000`/`$1800`, where the VIC sees character ROM whatever RAM holds.
  Its reverse-video half is mostly glyphs the game never draws, and
  `charset_install()` copies only the ones it does, so three runs of them are
  free for read-only data: `CHDATA1` (112 bytes, `$3C90`), `CHDATA2` (248,
  `$3D08`) and `CHDATA3` (448, `$3E40`), each a linker segment of its own. A
  module puts its rodata there with `#pragma rodata-name ("CHDATA3")` after
  its includes (inn, sprite and the lot are there, and so are a few strings of `q4k.c` and
  `help.c` that did not fit anywhere else, as `static const char[]` between
  two pragmas; they leave 9 bytes free in `CHDATA1`, 2 in `CHDATA2` and 8 in
  `CHDATA3`; the land and town
  generators' tables are in `CHDATA1`). `$A0` is the input cursor, `$C0-$C7`
  the bow shot's sprite block, and `$80-$91` the actors: leave those alone,
  and check `assets/tiles.txt` before using any glyph above `$91` on screen.
- `$4000-$49EB` **OVL**, the overlay: 2540 bytes at the bottom of HIGH that
  are two things in turn. What runs once at start-up lives in it -- the
  title screen (`title.o`), `charset_define.o` and the glyph table with
  `tiles_install()` in `tiles_data.c` (marked by `tools/mktiles.py`), all in
  the linker segments `TITLECODE`/`TITLEDATA`, which the Makefile gives them
  (`cfg/c64-tool.cfg` has the same two so the small tools still link) -- and
  afterwards the same bytes are scratch: the fight's arrays (`field`, `wall`,
  `roomy`, `seen` and the two queues in `battle.c`) and the dialogue's `reply`
  buffer, which `src/lib/overlay.s` names. None of them holds anything from
  one fight, or one turn of talk, to the next, so each may be used freely, but
  **nothing may start a fight or a conversation before start-up is done (the
  title screen and `view_init()` have returned), and none of the start-up code
  may run twice.** `__OVLSIZE__` in the cfg is the arrays' total and BSS
  starts that much higher; it must be at least what start-up needs (the
  linker says if not), and any excess is wasted, so when you move more
  start-up code in, alias more scratch to match, and change the sums in
  `overlay.s`. Padding after the start-up code is in the PRG. Moving it out
  freed about 2K of HIGH and 500 bytes of MAIN.
- `$49EC-$C1EB` **HIGH** -- `HIDATA` for bulk tables (the towns) and `HICODE`
  for module code that will not fit below. Modules listed in the Makefile's
  `HIMODS` are compiled with `--code-name HICODE`; add to that list when MAIN
  overflows. When HIGH is the one that overflows and MAIN has room, add the
  module to `HICODEMODS` as well: its code stays in HIGH but its rodata goes
  back to MAIN (the shop and the ships are there already). Changing either
  list needs the objects rebuilt, because a target-specific `CFLAGS` will not
  invalidate them on its own. About 212 bytes are free in HIGH, 214 in MAIN (plus
  the dead ONCE) and 13 in BSS, and the title screen has 29 to spare in OVL. The Makefile's `SMALLMODS` (loot, shop, inn, help, battle,
  dialog and dungeon) are
  built with `-Or`, not `-Osir`: without `-Os` cc65 stops inlining shifts and
  the like, which is about a tenth smaller but pulled some runtime helpers
  into MAIN (the figures above are after that). The fight is in it on the
  grounds that a turn-based flood fill can afford to be slower; nothing
  whose timing was measured is. Changing `cfg/` relinks everything.
- **The offline build** (`make offline`, `-DQ4K_OFFLINE`, in `build/offline/`)
  has no `dialog.o` or `modem.o`, so nothing opens the RS-232 buffers: it
  links with `__RSBUF__ = $CF00` and `__OVLSIZE__ = $0BEC` (`OFFLINE_OVL` in
  the Makefile), and the extra 512 bytes of overlay hold the dialogue files
  `talk.c` loads from disk. HIGH has about 1,470 bytes free there. Measure
  both builds after a change to either; `tools/screendump.py` finds
  `build/offline/q4k.lbl` for `dist/offline/q4k.prg`.
- **The C stack is at the top of MAIN, not where the cfg used to say.** cc65's
  startup points `sp` at `__MAIN_START__ + __MAIN_SIZE__`, `$3800`, and it
  grows down through MAIN's spare bytes (about 214 now) and then the dead ONCE
  segment. Painting `$3700-$37FF` and running a save and a long walk found
  the stack no deeper than 31 bytes, so what is left is plenty, but measure
  again before spending more of it. The 2K the cfg
  reserved at `$C700-$CEFF` was never touched, and is now BSS and the RS-232
  buffers: the buffers are at `$CD00`, `MEMSIZ` is `$CF00`.
- `$CF14-$CFFF` **BSS2**, 236 bytes of the probe page the probe does not use
  (it keeps `$CF00-$CF13`; `screendump.py` reads that much). Above the RS-232
  buffers, so it is not contiguous with BSS, and ld65 does not clear it: put
  something there with `#pragma bss-name (push, "BSS2")` only if it is always
  written before it is read. `fov_lit` (192 bytes) is there, and `__HIGHTOP__`
  in the cfg is raised by the same amount, which is where HIGH's extra room
  came from; 44 bytes of BSS2 are left. Anything that needs the probe page
  longer must move the bound in both the cfg and `screendump.py`.
- `$B800-$CCFF` **BSS**, then the RS-232 buffers and the probe page at
  `$CF00`. HIGH's top 4K is RAM under BASIC ROM (banked out at startup),
  taken from BSS.
- **The RAM under the ROMs** (`src/lib/vault.s`). `$D000-$DFFF`, under the I/O
  area, holds the home continent for the whole game, 64 by 64 bytes of tiles;
  `$E000-$FFF9`, under the KERNAL, holds a continent put away while a town or
  a dungeon is in `local[]` (four 1984-byte slots, `$800` apart; only slot 0 is
  used). Reaching either takes the machine to all-RAM (`$01 = $34`), which
  takes the KERNAL and the I/O area away, so interrupts are off for as long as
  it lasts and the only code that may run is in ordinary RAM and calls nothing
  of the KERNAL's. An NMI cannot be turned off and its vector is read from RAM
  under the KERNAL too, so `vault_enter` points it at an RTI. Everything is a
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
towns' is `tgen.s` (next to `town_gen.c`).

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
place game data there. The C stack (2K) lives directly below it.

**Colour RAM is write-only-ish nybbles at `$D800`** and is *not* covered by the
VIC bank switch — it stays at `$D800` regardless of where the screen matrix is.

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

`assets/theme.txt` is the score (`~` slurs a note into the next, `v` adds
vibrato); `tools/mkmusic.py` turns it into `src/lib/music_data.s`, and
`src/lib/music.s` plays it from cc65's interruptor chain (the
KERNAL's 50/60 Hz IRQ), in pure assembly that touches no zero page: a
triangle lead on voice 1, and on voice 2 a quiet triangle (the pulse it started as was too loud
and too bright) holding one
note a bar, a perfect fourth below whichever of the bar's melody notes leaves
the least clashing, resting for the bar's last eighth
(`HARMONY_REST = False` in the generator never re-strikes it: only the pitch
changes). A quick attack on a quiet SID voice clicks, because the envelope
always climbs to full level first; voice 2 uses a 100 ms attack for that
reason. (`HARMONY = "long"` in `tools/mkmusic.py` moves it only on
long notes instead, and `h` marks in the score force a move there). The generator works the harmony out; the score is just the tune. The player
and data live in HIGH (what is free now is under Memory). `music_pause()`/`music_resume()` wrap the disk
code, which the interrupt would steal cycles from. Pausing zeroes the master
volume *and waits twelve jiffies*: with the drive emulated, VICE makes no new
sound while the drive works and replays its last audio buffer, so a note
sounding at the moment the disk starts is held for as long as the disk takes
(a five-second drone on a save; the SID itself had been told to stop, as
`-sounddev dump` shows). Silence has to reach that buffer first. `M` toggles it. To hear it
without a human: `x64sc ... -sound -soundrecdev wav -soundrecarg out.wav`
with the game past its title screen -- `-keybuf` does not get there, so drive
it through `tools/screendump.py` with `-warp` and `-silent` stripped.
`--peek music_state` and `--peek music_idx` show whether it is playing. (If
VICE's coreaudio device is unavailable -- Bluetooth switching, say -- use
`-sounddev wav -soundarg out.wav` instead of `-soundrecdev`: it writes the
file without needing a sound card.)

Sound effects are on voice 3, from the same interrupt, and work whether or not
the music is on (`M` mutes only the music, `X` only the effects; both are handled in the
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

## Testing

**Do not run the full suite (`make test`) locally.** Push a branch and let
the GitHub Actions workflow (`.github/workflows/test.yml`, which runs on every
push to any branch) run it, and watch that result instead.

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

## Testing

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
in its text dump, which is how the goldens still record where the hero is
standing; `--raw` and `--hex` show plain screen RAM. `--shot` draws sprites
too. Any screen that takes over from the map must call `sprite_hide()`.

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

`--peek SYM[+OFF][:WIDTH]` prints any global after the run (wider than 8
bytes, just the bytes as hex, last first); `--poke
SYM=VALUE` (and `SYM+OFFSET=VALUE`) writes one before it. Both use the label
file ld65 emits next to the binary. Prefer it over adding debug
hooks to the game: the quest, the inventory and the party struct are all
reachable from a test this way, and none of it ships.

**The modem.** `tools/screendump.py --rs232 PORT` wires the user port to a TCP
socket and drops warp mode. `host/npcd.py --mock` serves deterministic canned
lines from `assets/npcs.json`, which is what the dialogue golden test uses —
never point a golden test at a live model.

**The dialogue backend** is a local Ollama model over its OpenAI-compatible
endpoint, called with plain `urllib` so `host/` has no third-party
dependencies. Prompts are assembled host-side and keyed by the id the C64
sends; the protocol carries no prompt text. Small models need the output
policed rather than requested — see `for_c64()`, which enforces the sentence
cap, strips stage directions and speaker labels, and clamps to ASCII. That
policing applies to model output only; authored mock lines pass through.
