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
  buffer (`talkbuf.s`, `TALKBSS`). Each scene's music is a file too (`m.map`,
  ...: every score is the same fixed size, see `tools/mkmusic.py`). The game
  needs its disk, and asks for it back if a load fails. The map's engine is
  the biggest, 15638 bytes of the slot's 17408, then the fight's (14277). The
  shop's engine also holds the screens the map opens that are not a place:
  the help screen and the party screen (`I`), through `shop_entry()` with
  `SHOP_HELP`/`SHOP_PARTY`; the quest's next-step table (`quest.c`) is in the
  core, since both the map and the help screen print it.
- `$8400-$C0FF` **HIGH** -- the rest of the core: `HIDATA` for bulk tables
  and `HICODE` for code that will not fit below. Core modules listed in the
  Makefile's `HIMODS` are compiled with `--code-name HICODE --rodata-name
  HIDATA`; an engine's modules go to their engine whatever that list says.
  Changing either needs the objects rebuilt, because a target-specific
  `CFLAGS` will not invalidate them on its own. About 5.3K are free in HIGH
  (4.6K online), 5.4K in MAIN (less the stack) and 247 bytes in BSS -- but
  **only 10 online**, so anything new that lives in BSS has to be paid for
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
place game data there (bar BSS2, above).

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

## The smooth-scrolling prototype

`src/openworld.c` (`dist/openworld.prg`, `make run PROG=openworld`) is a
standalone experiment, not the game: Keldmarch from the game's generator,
walked with a camera that glides a pixel or two a frame over 16x16
multicolour metatiles (2x2 characters), for the TeensyROM+/REU ideas. It is
one PRG with no disk, its own memory map (`cfg/c64-openworld.cfg`: VIC bank 1
at `$4000` for two screens, the charset and the hero sprite, home copied out
of the vault to `$B000`, its coasts at `$A000`) and its own raster interrupt, which chains to
`$EA31` with the CIA's timer interrupt off. The game links none of it.

`src/lib/smooth.s` explains the engine: the back screen is the front shifted a
character (`sm_shift`, about 10K cycles) plus the new row and column from the
map (`sm_edge_row`, about 6K, `sm_edge_col`, 4K), each in a frame of its own; on the frame the fine
scroll wraps, the interrupt at line 250 flips `$D018` and rewrites colour RAM
top down from the new screen (about 15K cycles), which keeps ahead of the
beam because each character has one colour (`ow_chcol`), so colour RAM is a
function of the screen. Measured (`--peek sm_rowend:25`, the low byte of the
line each row finished on): the last row is done at line 183 on PAL, about
60 lines before the beam needs it, and at line 228 on NTSC, about 12. NTSC is
the tight one: with the shift and the edge in the same frame, the frame after
a flip overran on NTSC (18 frames missed in 27 steps) and never on PAL. To
run on NTSC, `screendump.py --vice=-ntsc`. The camera
rests 4 pixels into a character so the first wrap of a step is two frames
away either way; steps are a square each, as in the game, so the direction
of every shift is known in advance. Eight ways (Q, E, Z, C or the stick's
diagonals; directions 4-7 after `sprite.h`'s four): a diagonal step is one
diagonal shift per flip, and on a mixed one (down-left, up-right), whose two
fine scrolls would wrap two frames apart, `glide()` holds the positive axis a
pixel back until the last frame so that they wrap together. A diagonal may
not squeeze between two corners of sea or mountain (`can_step()`). **Never build into the back screen in
the frame a flip was committed**: it is on show until the interrupt.
`late` counts frames the main loop missed (0 over long walks at both
speeds, PAL and NTSC); the loop waits for the frame after the last one it handled, not for
the next interrupt, or a build that runs past one costs a whole frame.

**The REU path** (`smooth.s`, after the copies). With an REU the shift is two
transfers (the front screen out, then back into the back screen `off` bytes
along: what wraps round a row lands in the edge column, which is redrawn
anyway), colour RAM is stashed with it, and the flip's interrupt fetches
colour RAM back the same `off` along and colours only the new edge. The
interrupt then ends about 48 lines after it starts instead of 245, so colour
RAM is done before the screen starts on PAL and NTSC, and a walk leaves
about half as much time again free on PAL, three quarters on NTSC (idle-loop
turns, `--peek idle:4 --peek walked:2`). `sm_reu_detect()` checks the
registers, a round trip, and that a transfer lands in colour RAM (in its
last 24 bytes, which nothing shows) -- the TeensyROM question -- and the
loading screen says what it found; `R` toggles it, `--poke use_reu=0` turns
it off. Each transfer in the main loop has interrupts off for its own setting
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

`--peek SYM[+OFF][:WIDTH]` prints any global after the run (wider than 8
bytes, just the bytes as hex, last first); `--poke
SYM=VALUE` (and `SYM+OFFSET=VALUE`) writes one before it. Both use the label
file ld65 emits next to the binary. Prefer it over adding debug
hooks to the game: the quest, the inventory and the party struct are all
reachable from a test this way, and none of it ships.

**The game needs its disk in every test past the title screen**, since it
loads its engines from it: the `.cmd` files pass `--disk dist/q4k.d64` (the
online cases swap in `dist/online/`), and the save tests start from a copy
of that disk, not a blank one. Without it the game stops after the title
screen with "Put the game disk back".

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
