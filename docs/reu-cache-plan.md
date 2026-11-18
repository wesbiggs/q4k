# Plan: a smaller engine core, bigger overlays, and an REU load cache

For a session picking this up cold. Read CLAUDE.md first (the open world's
sections: "The smooth-scrolling prototype", "The open world's engine is a
core and three overlays", "The screens of text are a program", "Fights in
the open world", and "Testing" twice). Everything here is the open world
(`openworld.prg`, `dist/openworld.d81`); the game proper (`q4k.prg`) is not
touched.

**Base:** branch `claude/gallant-cannon-mke4f0` at `9434335` ("SPACE gives
the window's rows to the map; castle chests; loads hidden"), which is about
to be merged to `main`. If it has been merged, start from `main`. Every
number below was measured from that commit's `build/ow-cpu.map`, `build/ow-reu.map` and
`make`'s `owslots:` lines; measure again before relying on one.

## Why

1. **Loads.** The engine leaves and comes back for every talk, shop, inn,
   party screen, help screen and fight (`owtown`, 33K, then `ow-data`, 10.7K, then the
   engine and its `.hi` file, then the area's overlay), and loads an overlay
   (4.5K) at every town or dungeon gate. With an REU, each of those can be a
   DMA copy after the first time: a 33K program takes about 33 ms.
2. **Room.** The engine is full. ow-cpu has **1 byte** free in MAIN (CODE
   plus MAPCODE end at `$3FFE`; ow-reu has 535), 8 in OWLO, 21 in HIGH, 12 in
   COMMON, and the slot is 4573 (world), 4584 (town) and **4608 of 4608**
   (dungeon). The cache's loader needs about 150 bytes in the engine, and
   there are none. The rest of the plan makes room by moving whatever only
   one kind of area uses out of what is always in memory.

## What is resident but used by one kind of area only

| What | Bytes | Where now | Used by |
|---|---|---|---|
| `ts_overworld` | 2101 | COMMON (`ow-data`, `$7BE9`) | world overlay (`back_home()`) |
| `ts_town` | 1779 | COMMON (`$841E`) | town overlay (`build_area()`) |
| `ts_castle` | 1869 | COMMON (`$8B11`) | town overlay |
| `ts_dungeon` | 1636 | COMMON (`$925E`) | dungeon overlay |
| town maps (`MAPDATA`) | 1009 | COMMON | town overlay (`town_gen()`) |
| `isle_gen.o` | 1328 | MAIN (`MAPCODE`) | the world, through `ow_gen.s`'s thread |
| `tgen.o` + `town_gen.o` | 902 | MAIN (`MAPCODE`) | town overlay |

A tile set's blob is read only while `sm_tileset()` (`smooth.s`) unpacks it
into the engine's tables in BSS and the character set; nothing reads it
afterwards (checked: the only callers are `world.c`'s `back_home()`,
`town.c`'s and `dungeon.c`'s `build_area()` calls). So the 7.4K of tile sets
need not be resident at all.

## Phase 1 -- tile sets out of `ow-data`

- `tools/mkoverworld.py` writes each set as a file of its own:
  `t.overworld`, `t.town`, `t.castle`, `t.dungeon`, on the D81. Keep the
  shared tables (`ow_pass`, `ow_qvar`, `ow_rnd`, `ow_sprites`) in `ow_data.s`
  (COMMON).
- The overlay that wants a set loads it into a scratch buffer just before
  `sm_tileset()`. **SHORE (`$A000-$AFFF`) is the candidate**: every path
  rebuilds it afterwards (`sm_shores()` for a place and for a fresh
  overworld; `sm_reu_keep(0)` puts back WORLD and its edges on the REU path).
  Verify that order in `back_home()` (`world.c`, where `sm_tileset()` comes
  before `sm_reu_keep(0)`/`fill_window()`, which is right) and in
  `build_area()` (`engine.c`), and check nothing reads SHORE between the load
  and the rebuild (the world's stream jobs are drained on the way into a place).
- The load goes through the engine's `load3()` (later the cache) with an
  explicit address, and through `load_failed()` if it fails, like an overlay.
- **What you get:** `ow-data` goes from 10.7K to about 3.4K, so every
  `ow_back()` reloads 7K less, REU or no REU. The cost is one ~2K load on
  the way into a place, with the screen already blank.
- **Tests:** every open-world golden must pass unchanged. The ones that go
  in and out of places matter most: `openworld-town`,
  `openworld-dungeon`, `openworld-out`, `openworld-dungeon-out`,
  `openworld-inn`, `openworld-throne`, `openworld-door` and their `-reu`
  twins. Add the four files to the disk's list in the Makefile, and to
  `tools/mkmemmap.py`'s `blocks()` if it lists `ow-data`'s contents.

## Phase 2 -- a bigger slot

Current engine map (`cfg/c64-openworld.cfg`): SLOT `$5A00-$6BFF` (4.5K),
LOCAL `$6C00-$73FF`, COMMON `$7400-$9FFF`. After phase 1 COMMON holds about
2.9K (RODATA 678, the shared tables about 2K, TALKDATA 90, CHDATA1 103;
MAPDATA moves to the town overlay in phase 3).

Proposed: **SLOT `$5A00-$7FFF` (9.5K), LOCAL `$8000-$87FF`, COMMON
`$8800-$9FFF` (6K, about 3K spare).** The exact split is the implementer's
call, but make the slot as big as COMMON allows.

Things that know the old addresses:
- `cfg/c64-openworld.cfg` (SLOTW/SLOTT/SLOTD sizes, LOCAL, COMHDR/COMMON).
- `src/openworld.c:214` (comment: `ow-data` "to its own address, `$7400`")
  and `cfg/c64-owboot.cfg`'s comment. The bootstrap's MAIN ends well below
  (`$280D`), and its BSS is at `$A000`, so a higher COMMON is safe there.
- `tools/owslots.py` (the slot's size, if it checks one), `tools/mkmemmap.py`
  (`blocks()`), `docs/memmap.html` (regenerated by `make`).
- `OVDART` is aligned at the slot's end (the dungeon monsters' art, aligned
  to a sprite block). Check it still lands where the VIC can see it: the slot
  is in VIC bank 1 (`$4000-$7FFF`), and `$8000+` is not. **Keep the art
  below `$8000`**: either the slot ends at `$7FFF` as proposed, or the
  art gets a segment of its own.
- `owtown` and the fight module load over everything above `$4000` and are
  followed by `ow_back()`, which reloads `ow-data` at its own address, so
  they don't care where it is. Confirm with `build/owtown.map`.
- CLAUDE.md's figures (the slot, the free bytes per region).

**Tests:** all open-world goldens. This phase changes addresses only, so
any diff on screen is a bug.

## Phase 3 -- the generators into their overlays

- **Town:** `tgen.s`, `town_gen.c` and the town maps (`town_data.c`'s
  `MAPDATA`) go into `OVTCODE`. Use the same trick as the dungeon's
  `build/ow/ow_dungeon.o`: a second build of the module into the overlay's
  segments, linked ahead of `q4k.lib` so the library's `MAPCODE` copy is not
  pulled in. Remember "cl65 makes `foo.s` from `foo.c`" in CLAUDE.md: build
  the copies through `build/ow/`, like `OWPART`.
- **World:** `isle_gen.s` and the build half of `ow_gen.s` go into
  `OVWCODE`. The thread switch (`gen_run()`, the frame interrupt's
  take-back, the stack and zero-page swap) can stay in the core. **The core
  may reach an overlay only through `ovl.s`'s jump table**, which
  `tools/owslots.py` enforces, so add an entry for the build (e.g.
  `ov_gen`). The other overlays point it at an `rts`; a build is already
  dropped on the way into a place. `isle_rnd.o` goes with it.
- **What you get:** the core's MAIN frees about 2.2K (ow-cpu: 1 byte to
  about 2.2K). The overlays end up at roughly 5.9K (world), 6.4K (town)
  and 3.9K (dungeon) of 9.5K.
- **Tests:** `make test` includes `tools/checkisle.py`, which builds lands
  on the C64 (`isletest`, a separate program) and compares them with the
  model. It is unaffected unless `isle_gen.s` itself changes, so don't
  change it, only where it is linked. Goldens: `openworld-sail` (continents
  built mid-voyage by the thread, with the `late` frame count:
  0 on PAL and with the REU, 4 on NTSC without, per CLAUDE.md; don't make
  it worse) and every town golden.

## Phase 4 -- the REU load cache (just in time)

A file is loaded from the disk the first time it is used, stashed in the
REU, and fetched by DMA every time after.

- **A loader module in `q4k.lib`** (`src/lib/ow_cache.s`, assembly), linked
  by every program that loads: the bootstrap, both engines, `owtown`,
  `owparty`, the fight module (`src/fight/`, `cfg/c64-fight.cfg`, which
  links no library now: link this one object). `ow_cload(name, addr)`:
  0 for the file's own address, as `cbm_load()`. It falls back to the
  KERNAL LOAD whenever there is no usable REU (`OW->reu` is `OW_NO_REU`),
  the cache is full, or the file is not cacheable.
- **Which loads:** `ow_run.s` (the engines, `owtown`, `owparty`),
  `ow_back.c` (`ow-data`), the engine's `load3()` (overlays, `.hi`, tunes,
  the phase 1 tile sets), `ow_fight.c` (`fight`, tunes) and `fight.c`
  (`f.*`, `s.*`). **Never `ow-save`**, which changes: the bootstrap's F3
  path and the engine's F1/F3 keep calling `cbm_load`/`cbm_save` directly.
  Splash pictures, `m.*` tunes in the bootstrap: either way.
- **Both engines.** An REU whose transfers do not reach colour RAM
  (`OW_REU_BLIND`) runs `ow-cpu`, but plain RAM-to-REU transfers still
  work, so cache with any REU found. Only the scrolling needs `OW_REU_OK`.
- **The directory** is in the REU: bank 1, page 0, entries of 8 bytes
  (REU address 3, length 2, C64 address 2, a valid flag 1), indexed by a
  hash of the name (128 entries is 1K). A build check (Python, run by
  `make` over the disk's file list) fails if two cacheable names hash to
  the same entry; keep the hash's salt in one place the tool and the
  assembly share (a generated `.inc`). A lookup is one 8-byte fetch.
- **The data** goes from bank 1 past the directory upward, packed. The
  engine already uses bank 0 (`REU_SCREEN` `$0000`, `REU_COLOUR` `$0400`,
  `REU_KEEP` `$2000`, 8K): leave bank 0 alone. The bootstrap measures the
  REU's size (a byte written to each bank's first address, highest first,
  then read back) and puts the end in the directory's header; a file that
  would not fit is just not cached.
- **Stash at once.** After a miss, stash the image right after LOAD
  returns and before anything runs or writes into it: programs change
  their DATA, tunes their variables on init, the fight module's caller
  fills its block at `$5800`, and the fight draws over its field at
  `$4000`. The end address comes back from LOAD in X/Y; the start is
  the file's own address (check which KERNAL zero-page pair holds it
  after a LOAD at the file's own address) or the explicit one.
- **Tunes** load to `$E7C0`, under the KERNAL. A fetch writes the RAM
  there whatever `$01` says, but a **stash reads the KERNAL ROM unless
  `$01 = $35`**, so stash a tune with interrupts off and the KERNAL banked
  out (the music's interrupt already copes with `$35`; see the NMI note
  under Memory in CLAUDE.md, and use `vault_nmi` as `vault_enter` does).
  SHORE, the other scratch, is plain RAM.
- **`ow_run.s`'s stub** runs from the tape buffer (`$0334`, and must end before
  the disk sprite at `$0380`: there is an `.assert`). The cache path needs
  more: do the directory lookup in `_ow_run` (in the caller, before the
  copy) and give the stub only the fetch or the load-and-stash. If it still
  does not fit, the BASIC input buffer at `$0200-$0258` is free during a
  hand-over (the engine's `said` line lives there, but only while the
  engine runs).
- **When the cache is cleared:** on a cold start of the bootstrap, unless
  the directory's header has a magic and a **stamp that matches this
  build**. `make` computes the stamp (a hash of the cacheable files'
  contents) and writes it into a header the bootstrap includes. That keeps
  a preloaded image (below) and guards real hardware against an REU left
  over from an older disk. `N` and F3 go back through the bootstrap with
  `OW->magic` set, and keep the cache.
- **DMA and the raster:** a fetch stops the CPU for a cycle a byte, so the
  4.5K-9.5K of an overlay is about 75-150 raster lines, and the 33K of
  `owtown` two frames. The screen is blank or the disk icon is up in
  every case today. Anything that fetches while the map shows calls
  `sm_guard()` first (see "The text window" in CLAUDE.md).
- **A counter for tests:** `cache_hits`/`cache_misses`, non-static so they
  get labels for `--peek`.

**Tests:**
- every `-reu` golden passes unchanged (the REU's cache starts empty in
  them, so they test the miss path);
- a new case in `tools/test.sh`: with an REU, go into Millford and out
  twice (or talk to Old Tam twice), and `--peek cache_hits` is non-zero and
  the screen matches `openworld-out`;
- `ow-cpu` with an REU: there is no way to get that today (VICE's REU
  passes the colour-RAM check), so add one for tests (below).

## Test acceleration

Two independent pieces. Measure the open-world goldens' wall time before
and after each (CLAUDE.md: "about six minutes for all of them", four VICEs
at a time).

1. **Loads served by VICE, not an emulated 1581, for most goldens.**
   `screendump.py`'s `launch()` already does this for `--rs232`
   (`+drive8truedrive -trapdevice8`, line ~296). Add an option (say
   `--fastdisk`) and use it in most open-world `.cmd` files. **Keep true
   drive emulation** in the cases whose point is the drive: `openworld-talk`
   (the RUN/STOP during LOAD that `nostop` and `ow_back()` work round),
   `openworld-save`/`openworld-load`, `openworld-party` (F1 through the
   bootstrap), and one overlay switch each for `ow-cpu` and `ow-reu`. Also
   check that `cbm_save` and the scratch (`savegame.c`'s way) work under
   trapping, or leave every case that saves on the true drive.
2. **A preloaded REU.** VICE loads an REU's contents from a raw image:
   `-reu -reusize 512 -reuimage FILE` (without `-reuimagerw`, nothing is
   written back, so every run starts from the same image). A tool
   (`tools/mkowcache.py`, run by `make`) writes `build/owcache.reu`: the
   directory, the stamp and every cacheable file, laid out exactly as the
   loader would. `screendump.py --reucache` passes those flags. Use it
   for most `-reu` goldens; **keep the existing `-reu` goldens' runs cold**
   (or at least a few of them) so the miss path stays covered.
   - **`ow-cpu` with a cache:** a test-only hook the bootstrap reads before
     choosing the engine (`src/openworld.c:265`), written by the harness
     the way `--splash` writes `OW->hook` while the splash waits: e.g.
     `--cpu-engine` makes the bootstrap run `ow-cpu` with `OW->reu`
     left as found. Then the `ow-cpu` goldens can use the preloaded cache
     too.
   - What it does not speed up: the bootstrap's own work (the splash,
     building home and the continents: several seconds of the ~15).

Order: 1 needs no code in the game, so it can land first, even before
phase 1. 2 needs phase 4.

## Optional, later

- **A transient overlay for rare events.** Save, load, quit, and
  `load_failed()`, with what only they use (`cbm_save`, `cbm_read`,
  `cputs`/`cputc`, `cgetc`, `itoa`/`udiv`: about 0.6-1K of the core's
  CODE), swap the area's overlay out and back (its BSS stays, as with any
  overlay switch). Only worth it if the core is short of room again after
  phase 3. With the cache, the swap is two fetches.
- **`owtown` split per screen.** It is 33K, loaded whole for every talk,
  shop, inn, party and help screen. With the cache that is about 33 ms, but
  without an REU it is a full disk load every time: one program per
  `OWB_*` mode (or talk on its own and the rest together) would help
  machines without one. Watch `build/owtown.map`'s module list: CLAUDE.md
  warns that one stray reference pulls 8K back in.
- **`ow_sprites`** (640 bytes in COMMON): if it is only copied into the
  sprite blocks at `$5000` at the start, it can go the way of the tile
  sets. Check before moving.

## Rules that apply throughout (from CLAUDE.md)

- Don't run `make test` locally; push and let CI run it. A cloud session
  can run single goldens after `sudo tools/cloud-setup.sh`.
- After any change that moves the core, `make` rebuilds the D81 for the
  open world, but check `dist/openworld.d81` is newer than the engine
  before a test.
- Re-bless goldens deliberately: phases 1-3 should change no screen.
- `tools/owslots.py` and `tools/mkengines.py` fail the build when
  something calls into an overlay or engine the wrong way. Keep them
  passing; don't loosen them.
- Update CLAUDE.md as each phase lands (the open world's Memory paragraph,
  the overlay paragraph, "The music carries on", Testing), in its
  existing style, and remove anything it says that stops being true
  (e.g. `ow-data` "10.7K at `$7400-$9FFF`").
- Commit each phase separately, and push after each so CI runs.

## Expected result

| | Now | After 1-3 | With the cache (repeat visits) |
|---|---|---|---|
| ow-cpu MAIN free | 1 B | about 2.2K | about 2.0K |
| Slot | 4.5K, dungeon full | 9.5K, at least 3K free each | same |
| `ow-data` reloaded by `ow_back()` | 10.7K | about 3.4K | a fetch |
| Back from a talk or shop | 4 disk loads (`ow-data`, engine, `.hi`, overlay) | the same, 7K less | 4 fetches, well under 0.1 s |
| Into a town or dungeon | 1 load (4.5K) | 2 (overlay and tile set) | 2 fetches |
