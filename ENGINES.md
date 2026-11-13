# Engines: what could come off the disk

The first part of this file is the checkpoints as built: approach B, first
in the offline build and then in both. The rest is the exploration that led to it, kept as it was
written: with a fast drive (TeensyROM's vdrive), how much memory can the
offline build free by keeping only a small core resident and loading each
mode's code and data when that mode starts? Those figures are measured from
the offline build as of 0.11.0 (`make offline`, then `tools/modes.py`).

## Checkpoint 2: both builds

(Since this was written, the offline build has become the game: `make` builds
it into `build/` and `dist/`, and the networked one is experimental, `make
online` into `build/online/` and `dist/online/`, with `Q4K_ONLINE` in place of
`Q4K_OFFLINE`. Releases ship `q4k-<version>.d64`, the offline game, and the
online build as `-online.d64` and `-online.zip`.)

The networked build now works the same way: the same engines, the same
memory layout (`cfg/c64-q4k.cfg`, which both link), the same disk files. The
two builds differ only in who talks:

| | networked (`make`, `make d64`) | offline (`make offline`, `make d64-offline`) |
|---|---|---|
| in the talk's engine | `dialog.c`: the conversation over the modem | `talk.c`: lines from the disk, and its buffer (`talkbuf.s`) |
| in the core | `modem.c`, and `line.c`: the title screen's call to the host, and the reply buffer | nothing |
| on the disk | the PRG, the engines and the music | the same, and everyone's lines |

`Q4K_OFFLINE` is tested in two places: the title screen's call to the host,
and the networked half of `include/q4k/dialog.h`. Everything that used to be
offline only for want of room -- the continents' own monsters, the arms
monsters drop, lightning, the joystick -- is in both, and the online build's
own flat arrangement (the `OVL` overlay that the title screen ran from and the
fight's arrays reused, `HICODEMODS`) is gone.

What changed to get there:

- **`dialog.c` is in the talk's engine**, with the same interface as `talk.c`:
  `dialog_talk(npc, name, given)` and `dialog_hint(inn)`. The ids the host
  knows people and inns by (`npc_ids`, `inn_ids`) are only read there, so
  `tools/mktowns.py` puts them in `TALKDATA`. `talk_entry()` moved to
  `gift.c`, shared: with no host on the line, or no file on the disk, it hands
  over what the quest needs and the map says so ("has nothing for you" if
  nothing).
- **The probe is in the core.** The title screen asks for the host before any
  engine is loaded, from the slot itself, so the reply cannot land in the slot:
  `line.c` (the probe, the reply buffer, reading an answer) is core, as is the
  modem driver.
- **The RS-232 buffers keep `$CD00-$CEFF` in both builds**, so that the
  layout is one file. The offline build gives up the 512 bytes of BSS it had
  taken; HIGH, which has room to spare, ends lower to make up for it
  (`__HIGHTOP__` is `$C100`).
- **The test programs** that link the game's modules (dungeontest, arena,
  isletest) are compiled from the same objects, with the engines' segment
  names, so they link with `cfg/c64-flat.cfg`, which puts every engine's
  segments in HIGH in one load. `view_place_fn` may be unset there (the arena
  has no map), and the panel then leaves "where you are" blank.
- **Tests.** The online screen tests attach `dist/q4k.d64`. The save tests
  start from a copy of it rather than a blank disk, since the game cannot
  leave the title screen without its engines. The dialogue tests run at real
  speed (the modem cannot be warped), where an emulated 1541 would take a
  minute for each engine, so `screendump.py` serves the disk from VICE's
  KERNAL traps instead when `--rs232` is given. The fight test runs against
  both builds.
- **Releases** publish the `.d64`s and the zip (which holds the game's disk);
  a PRG alone would stop at the title screen. `tools/play.sh` plays from a
  copy of the game's disk, `dist/saves.d64`, and carries `Q4K.SAV` over when
  the game disk is newer.
- **One behaviour change online**: the tune restarts at every fight and
  conversation, as it already did offline (Checkpoint 1, choice 5).

What is free now, measured from the two maps:

| Where | networked | offline |
|---|---:|---:|
| HIGH, to `$C0FF` | 7253 | 7974 |
| MAIN, to `$37FF` (the C stack grows down from the top) | 5961 | 5996 |
| BSS, to `$CCFF` | 197 | 434 |
| CHDATA1 / CHDATA2 / CHDATA3 | 9 / 125 / 114 | 9 / 91 / 114 |
| BSS2 | 29 | 29 |

| Engine | networked | offline |
|---|---:|---:|
| title (in the PRG) | 2517 | 2352 |
| map | 17089 | 17089 |
| fight | 10917 | 10917 |
| talk | 2872 | 5325 (with its 3052-byte buffer) |
| shop | 2021 | 2021 |

The slot is 17408 bytes, so the map has 319 to spare.

## Checkpoint 1: approach B, built

The offline build (`make d64-offline`) now keeps a **core** in memory and
loads everything else into one **slot** at `$4000` from the disk, one engine
at a time. The online build is the same game as before. It links the same
split-up sources the old way, and its tests cover that.

| Engine | File | In the slot | Holds |
|---|---|---:|---|
| title | (in the PRG) | 2352 | the title screen and the charset's set-up, as before |
| map | `e.map` | 17179 | `q4k.c`'s loop, scenes, drawing, FOV, towns, loot, treasure maps, help, sailing, the land/town/dungeon generators, the fight wrappers |
| fight | `e.fight` | 10917 | battle, arenas, `open9`, and the fight's arrays (2360) |
| talk | `e.talk` | 5181 | talk, inn, gifts, and the dialogue file buffer (3052) |
| shop | `e.shop` | 2021 | the shop, and taking up dropped gear |

The slot is `$4000-$83FF` (17408 bytes, sized to the map with 229 to spare).
What is free in the offline build now:

| Where | Free | Was |
|---|---:|---:|
| HIGH, `$A1DA-$C3FF` | 8742 | about 400 |
| MAIN, `$2094-$37FF` (the C stack grows down from the top) | 5996 | about 160 |
| BSS | 179 | about 60 |

That is about 14.9K, all in the core and so usable from every engine. The
slot itself has room to grow too: the fight has 6.5K spare in it, and talk
12K.

### How it works

- `cfg/c64-offline.cfg` has the slot as five memory areas at the same
  address. One is in the PRG and holds the title screen; each of the others is
  written to a file of its own, with a two-byte load address in front.
  `tools/mkengines.py` renames them to `e.map` and so on, and the Makefile
  puts them on the disk.
- The Makefile's `ENG_MAP`, `ENG_FIGHT`, `ENG_TALK` and `ENG_SHOP` lists say
  which modules go where. Only an engine's **code and constants** move: its
  variables (BSS and DATA) stay in the core, so the map's statics are just as
  they were when it is loaded back after a fight. The fight's arrays and the
  dialogue buffer are the exception: they are scratch at the end of their own
  engine (`FIGHTBSS`, `TALKBSS`).
- `engine_call(e, entry)` (`src/lib/engine.c`) loads engine `e`, calls its
  entry, and loads back whatever the slot held before returning. Because the
  map comes back byte for byte at the same address, a call from deep inside
  the map's code (`try_move` → `wander_check` → `battle_wander`) returns
  normally, and no call chain had to be restructured.
- `tools/mkengines.py` runs after every offline link. It fails the build if
  any module uses code or constants in an engine that may not be loaded:
  the core or another engine may only reach an engine's `*_entry`. That
  check is what makes the arrangement safe to keep changing.
- `src/lib/boot.c` is the offline `main()`. It runs the title screen from the
  PRG, loads `e.map` over it, and calls `map_entry()`, which is `q4k.c`'s old
  `main()` loop.

### Choices and assumptions to review

1. **Offline only.** (Since Checkpoint 2, both builds.) The online build
   keeps one flat link, as before. The
   source was split so that each engine holds only what it calls: `arms.c`
   (arms tables, dice, potions) out of `battle.c`; `scene.c` (building
   scenes, rebuilding after a load) out of `game.c` and `savegame.c`;
   `draw.c` (drawing the map, the panel's "where you are") out of `view.c`;
   `gift.c` (`quest_after_talk`) out of `q4k.c`. Both builds link the split
   files. In the online build MAIN went from 159 bytes free to 135, and HIGH
   from 57 to 39.
2. **Four engines, not one per screen.** Help, sailing, the generators and
   saving stay in the map's engine. Help shares its quest hints with the map;
   the others call back into map code (`isle_work`, `world_view`,
   `town_build`). Moving them out is possible but would mean restructuring
   those calls. The shop and the inn are separate because they only need the
   core.
3. **The inn travels with talk**, because an ale loads a rumour file through
   the talk code. The map works out the inn's or person's name and hands it
   over in `eng_text`, since the names are the map's data.
4. **A fight's spoils are shared out by the map**, after it is reloaded
   (`fightcall.c`): the treasure map, then dropped gear. This is the same
   order and the same dice as before. `gear_drop()`'s roll moved in front of
   loading the shop engine, so the shop loads only when something really
   drops. The messages still appear in the same order in the log.
5. **Music.** Each scene has a score file (`m.map`, `m.fight`, `m.talk`), all
   the same tune for now, cut out of the PRG by `mkengines.py`. A score loads
   in the same pause as its engine, and starts from the top. So **the tune
   restarts at every fight and conversation, and again on returning to the
   map**; before, it carried on. The shop has no score and keeps the map's
   tune playing. Every score takes a fixed 516 bytes (32 pitches, 128 notes:
   `MAX_PITCHES`, `MAX_NOTES` in `tools/mkmusic.py`), so they can replace one
   another.
6. **Sprites are hidden during every load** (the hero reappears on the next
   draw). The open question in CLAUDE.md, about sprites and the serial bus,
   matters more now that loads are frequent.
7. **The disk is required.** The game cannot leave the title screen without
   `e.map`. If a load fails, it says "Put the game disk back, then a key." in
   the log and tries again after each key. The `offline-nodisk` test now
   checks for that message; Garrett no longer hands the dagger over on the
   map with no disk.
8. **The title screen is still in the PRG.** The `OVL` arrangement and its
   "nothing may run twice" rule are gone from the offline build: the title is
   just what the slot holds first.
9. **Two loads per visit.** Every fight, conversation, inn and shop loads its
   engine and then the map again. With vdrive that is a fraction of a second
   each way, plus the 0.2 s music pause. With a stock 1541 it would be about
   40 s each way, so this build is for fast drives.
10. **Not reentrant.** An engine called through `engine_call()` must not
    make another `engine_call()`. Nothing does today, and the core's
    `static` keeps the rule simple.
11. **Talk with no file** (a disk missing one person's file): the gift is
    still handed over, and its line is copied back in `eng_text` for the map
    to show, as before.

### Tests

The online suite is unchanged. The offline cases are:

- `offline-title`, `offline-talk`, `offline-reward`, `offline-inn`, as
  before. Their settle times are longer to allow for the engine loads, which
  run with full drive emulation in warp mode.
- `offline-nodisk`: the disk prompt (see 7 above).
- The online screen tests `q4k`, `fov`, `loot`, `sail`, `sailhome`,
  `captain`, `treasure` and `treasuremap`, run against the offline build with
  its disk attached, against the same goldens. A
  `tests/golden-offline-<name>.txt` overrides one if the two builds ever
  differ.
- `offline-fight`: `L` raises the dead on the road (`grave_odds=1`), the
  hero has 1 HP and only guards, and must wake up in Millford. That covers
  the map → fight → map swap and dying.

### Play-test checklist

On the TeensyROM, from `q4k-…-offline.d64` (or `make d64-offline` and
`dist/offline/q4k.d64`):

- New game: the title screen, then a short pause while the map loads, then
  Millford with the music starting.
- Talk to Garrett (1, 2 ...) and leave with RUN/STOP; the map comes back as it
  was. Visit the inn: an ale's rumour, a bed.
- Buy something at a shop; the panel shows it on the way out.
- Leave Millford and fight a few times: winning (watch for "It had a
  treasure map!"), fleeing by a side gap, and dying (waking in Millford).
- Kill a few goblins or bandits for a dropped blade (1 in 12).
- Enter the Hollow Crown, go down and up a level, fight in a room.
- Sail to a continent and back; enter its town.
- F1 to save in a town and in a dungeon; F3 to load, and load from the title
  screen.
- M and X during a fight and on the map; the music should restart cleanly
  after each fight.
- Take the disk out (unmount it) and walk into a fight: the prompt should
  appear, and putting the disk back plus a key should carry on.

## The short answer

| Approach | Freed (about) | Load per fight | Time (vdrive / with DMA) | Rework |
|---|---|---|---|---|
| A. One swap slot; the map stays resident | 9K | the fight, ~9K | 0.18 s / 0.03 s | small |
| B. The map is swapped out too | 17K | the fight in, the map back, ~22K | 0.43 s / 0.07 s | moderate |
| Data only (towns, arenas, help text, FOV memory) | 2-3K | none | | small |

On top of all of that, **about 6K under the KERNAL is unused today**
(`$E7C0-$FFF9`, vault slots 1 to 3), and with the disk, slot 0 can be freed
as well.

Both A and B work through the same mechanism, a *far call*, and you can do A
first and B later. vdrive loads fast enough
([load speed](#load-speed)) that B's extra loading costs almost nothing.
That makes B the better target, because its one large slot suits every
feature on the list.

## Where the bytes are

`tools/modes.py build/offline/q4k.map`, grouped by which mode needs them
("loaded" is code and constants, which is what a disk file would hold):

| Group | MAIN | CHDATA | HIGH | OVL | BSS | loaded |
|---|---:|---:|---:|---:|---:|---:|
| core (always needed) | 4811 | 64 | 5586 | | 549 | 10461 |
| map (walking about) | 4077 | 91 | 4946 | | 1312 | 9114 |
| services (one at a time) | 475 | 439 | 10917 | | 679 | 11831 |
| fight | | | 9098 | | 219 | 9098 |
| start-up (already an overlay) | 62 | | | 2352 | 27 | 2414 |
| cc65 runtime | 2677 | | | | 28 | 2677 |

Plus the 3052-byte OVL scratch area, which the fight's arrays and the
dialogue file already share.

- **core**: view, text, panel, sprite, music and its data, vault, stick,
  probe, rle, actor, game state, and `local[]` (the 2K font image that becomes
  the map buffer).
- **map**: `q4k.c` (3.9K), loot (2.5K), fov (0.9K of code, 1.2K of BSS),
  town data (0.9K), world, town_gen, place_near.
- **services**: shop 2.0K, help 1.8K, dungeon builder 1.6K, sail 1.5K,
  isle_gen 1.4K, talk 1.1K, savegame 0.8K, tgen 0.8K, inn 0.8K. They add up
  to 11.8K, but **only one is ever running at a time**.
- **fight**: battle.c is 8.5K by itself: `fight()` 1.7K, `flood()` 1.1K, and
  1.5K of monster tables, arms tables and messages. Then arena maps 0.6K and
  open9.

## Which mode needs what

**Map mode** needs core, map, and sometimes one service. It never needs the
fight engine (9.1K) at all. In the overworld it never needs the dungeon
builder or FOV either, and in a town it never needs sail, isle_gen or loot's
digging.

**Fight mode** needs core and the fight: nothing in map, and no service. From
the link graph (`od65` on every object), the fight calls out of its own group
for only two things:

- `treasure_drop()` (loot, map) and `gear_drop()` (shop, service). Both happen
  after the fight is won, so they can become a flag that the map acts on when
  the fight returns.
- Some of battle.c is used by everyone else and would move to core: the arms'
  names and stats (view and shop read them), `rng_stir`/`reseed`,
  `encounter_roll`, `potion_heal`/`drink_potion`/`heal_full` (map, inn).
  That is about 0.7K. `isle_biome`, which is state, would move too.

**Talk mode** already comes off the disk: `talk.c` (1.1K, resident) loads one
person's file into the OVL scratch area. While someone is talking, neither
map nor fight is needed. The only links back are the gift callback
(`quest_after_talk` in `q4k.c`) and the inn calling `dialog_hint`.

**Services** are each called from the map and return to it. Their calls
outward go to core, plus a few into map (inn → `town_gen_inn`;
savegame → `dungeon_build`, `isle_build`, fov; sail → `isle_work`, world).
The generators (isle_gen, tgen, dungeon: 3.8K) are called from the map, from
sail and from savegame, so they work best kept resident with the map, or in a
slot of their own.

## The mechanism: a far call through a slot

A slot is a fixed range of memory. Each candidate for it is linked to that
address and written to its own file on the disk. Core keeps a note of which
file the slot holds, the way `talk.c`'s `held` does for dialogue files.

    r = far_call(FILE_FIGHT, entry, args)
      -> if the slot does not hold FILE_FIGHT, load it
         r = entry(args)
         if the slot held something else before, load that back
         return r

The point of reloading the previous image before returning is that **the
caller does not have to be restructured**. Say map code at `try_move()` →
`wander_check()` → `battle_wander()` calls the fight through a far call, and
the fight overwrites the map code. Before the far call returns, the map's
bytes are back exactly as they were, at the same address, so the return
address on the 6502 stack is valid again. That works because:

- With `--static-locals` and no recursion, a function's locals are static
  storage. Put each engine's BSS **outside** its slot (in core's BSS), and
  they survive the swap. The C stack is at the top of MAIN and survives too.
- An engine must have no `DATA` segment (initialised and then written), since
  reloading it would reset it. Constants (`RODATA`) are fine.
- A swapped-out engine must not be called. One `ld65` link makes every file,
  so addresses always agree, but `ld65` will happily resolve a call into an
  engine that is not loaded. A check in `tools/modes.py` could fail the build
  when core imports any symbol from a slot except its entry table.

`ld65` does this directly. Give several `MEMORY` areas the same `start` and
their own `file = "%O.fgt"` (and so on), and put each engine's segments in
its own area: cc65's own `overlaydemo` is built this way. The cc65 runtime
helpers that any overlay needs are linked into `CODE` once, in MAIN, so
engines share them. `cbm_load(name, dev, NULL)` loads a file at its own
address. Around the load, do what `talk.c` already does: raise MEMSIZ, and
call `music_pause()`/`music_resume()`.

### A: one swap slot, map resident

Resident: core and runtime (13.1K), map (9.1K), generators (3.8K).
One slot of about 11.6K, the size of the fight plus its scratch arrays. Any
one of these goes in it:

- the fight (9.1K, and its 2.5K of arrays as its own BSS inside the slot,
  since nothing in them survives a fight)
- talk, the person's file, and later a portrait (see below)
- shop, inn and talk's hint, help, sail, save and load
- the title screen and `charset_define` at start-up. This replaces the
  OVL arrangement and its "nothing may run twice" rule. OVL just becomes the
  slot.

The only call that has to change is the fight's `gear_drop()`: either move it
into the fight (it is 218 bytes), or have it return a flag. Load cost: about
9K per fight, 2K per shop. Nothing is reloaded on the way back, because
nothing resident was overwritten. **This frees about 9K of HIGH**, and
another 2-3K with the data moves below.

### B: swap the map out as well

Resident: core and runtime only, 13.1K. Most of it fits MAIN once view and
game are split: their map-drawing halves (`draw_local`, `world`, town
building) go with the map. One engine slot of about 16K holds either:

- the map engine (map, generators and a ~4K service sub-slot inside it), or
- the fight, or
- talk with a portrait.

When a far call returns into the map, the map has to be reloaded (~13K), so
every fight costs about 22K of loading. **This frees about 17K.** It also
gives the slot a clean shape for graphics (see the portraits below).

### Data that can come off the disk without moving any code

- **Town data** (919 bytes, two thirds of it in HIDATA): load one town's RLE
  map and entities when you go in.
- **Arena maps** (565) and **monster tables** (most of battle.c's 1.5K of
  constants): load per land or per dungeon, or load with the fight engine.
- **Help text** (1.1K): already only read on one screen.
- **FOV memory**, `fov_mem` (768 bytes of BSS). It remembers what you saw on
  the levels of the *last* dungeon only. Instead, write each level's 192 bytes
  to a file when you leave it, and you get every level of every dungeon
  remembered, for 192 bytes of RAM instead of 960.
- **The save blob** (266 bytes of BSS). This is a copy of the game state,
  made so it can be saved in one call. Keep the state contiguous in the first
  place (see below) and save it straight from where it lives.
- **The parked continent** (vault slot 0, 1984 bytes under the KERNAL).
  Continents are generated from the seed and take about 2 s to build, so they
  are parked rather than rebuilt. Writing the parked map to a file is just as
  fast and leaves the RAM under the KERNAL free, all 8K of it.

### Core state

"Keep minimal core state" turns into one rule: **anything that has to survive
a swap lives in core, in one contiguous block**. That block is then also the
save file. Today it is scattered across `game.c`, `actor.c` (the party
already has room for four), loot's `tmap`, `looted`, the fov memory and a few
module statics. It is a few hundred bytes. A four-person party with its own
inventories adds a few hundred more, which is cheap. Gathering it into one
block is the first piece of real work either approach needs, and it pays for
itself in savegame.c.

## Load speed

Measured on the hardware: vdrive's replacement KERNAL loads the whole 38K
`q4k.prg` in about 0.75 s, roughly 50K a second, and with DMA it is about
six times faster again. So a 9K fight loads in about 0.18 s (0.03 s with
DMA), and B's 22K round trip in about 0.43 s (0.07 s). VICE in warp mode is
a fair stand-in for that, so the screen tests need nothing special beyond
having the disk attached. (A stock 1541 manages about 400 bytes a second,
so this design is for vdrive, or another fast loader, only.)

At those speeds the fixed cost of each load matters more than its size.
The music has to stop for every load and save, and `music_pause()` waits
twelve jiffies (0.2 s) for the silence to take effect, so a swap costs
roughly 0.2 s of silence plus the load. A few things keep that from
showing:

- **A swap is a change of scene, and so a change of music.** With a tune
  for each scene, the pause is not a gap in a tune: it is where one ends and
  the next begins. Stop the old tune, load the engine, start the engine's own
  tune. The silence becomes the transition, a beat before the fight music.
- **One pause per swap, not one per file.** If an engine needs a second
  file (its score, a portrait, a dungeon level's memory), `far_call` loads
  them all inside a single pause.
- **Saves and the talk files keep the pause they have now.**

Other costs:

- **The disk becomes required for fights**, not only for talking (which is
  how the offline build already works: see `golden-offline-nodisk`). Every
  screen test that can reach a fight would need `-8 dist/offline/q4k.d64`
  attached, and the D64 gets a file per engine.
- **Sprites and the serial bus.** CLAUDE.md has an open question about
  whether three enabled sprites upset disk access. Loading much more often
  makes that matter, so hide the sprites during a load and settle the
  question.

## Your ideas, against this

**Party and per-character inventory.** The state is small and goes in core.
The weight is in the fight (turn order, choosing each member's action,
targeting) and the screens (inventory, equip, trade). With the fight in a
slot of its own it can grow by several K without touching the map, and the
inventory screens are just another service.

**Classes (warrior, archer, wizard).** Mostly data: an aptitude table per
class, which gear each may use, how levels grow. It costs a class byte per
character in core, and tables loaded with the fight.

**Spells and more potion types.** Spell effects in a fight belong in the
fight. Spells cast outside a fight (light in a dungeon, heal, a recall) are a
small service. Item and spell names and costs can be a data file loaded with
whichever of the two needs them.

**Monsters you can see on the map, dungeons included.** This is the one that
grows the map engine, not a slot. It needs a list of monsters for the current
map, a step for each per turn, drawing, and a fight that begins when one
reaches you instead of from a dice roll. The actor glyphs `$80-$91` already
draw people. A dungeon level's monsters can be saved to a file when you leave
it, like FOV memory, so that they stay dead. Under approach A this means
spending some of the freed 9K on the map itself. Under B the map engine has
the whole slot.

**Smoother, sprite-like movement.** Three of the eight hardware sprites are
in use (hero, tunic, the shot). That leaves five for monsters near the hero,
moved with the same per-pixel step `sprite_walk()` gives the hero. Monsters
further off can stay glyphs. More than five on screen at once needs a sprite
multiplexer, a raster interrupt, and that fights with the vault's
interrupts-off copies (up to 30 ms). Avoid that unless you have to.

**NPC portraits when talking.** While someone is talking, neither the map
nor the fight is in memory (approach B), or the slot is free (A). Two ways
to draw one:

- *Character mode, no new video mode.* Load the portrait as up to 128 custom
  glyphs (1K) over the charset's reverse half, plus colour RAM. Multicolour
  character mode gives each 8x8 cell its own colour plus three shared ones.
  This is cheap and needs no raster tricks. The cost: the reverse half
  currently holds CHDATA1-3 (808 bytes of constants, talk's own strings among
  them), the actors and the bow shot's sprite block. The constants would move
  into the slots this plan frees, and the glyphs would be put back after the
  conversation (`charset_install()` refuses a second copy today, so that
  needs a variant).
- *Bitmap.* If the engine slot starts at `$4000`, which is VIC bank 1 and has
  no character-ROM shadow, a talk engine can use the whole bank: a multicolour
  bitmap at `$6000` with its screen matrix inside the bank. Showing text under
  it needs a raster split back to bank 0's text screen, or text drawn into
  the bitmap. Higher quality, and a lot more code. This is the reason to put
  approach B's slot at `$4000` if you go that way.

**Bitmap tiles with their own foreground and background colours.** This
costs the most. Hires bitmap mode gives exactly what you want (each 8x8 cell
has its own two colours, and any number of tile shapes), but it needs 8K of
bitmap and 1K of screen in one VIC bank for as long as the map is showing.
The cheapest place is bank 3: the bitmap at `$E000`, under the KERNAL. Writes
go through to RAM underneath without banking, so drawing is plain stores, and
that is the RAM the disk frees above. The screen matrix would be at `$CC00`
(the offline build has no RS-232 buffers to keep there). The hero's sprites
would move into bank 3, and every piece of text (log, panel, menus) would
have to be drawn into the bitmap or kept to a text-mode band with a raster
split. Do it after the slots exist, and prototype the drawing speed first.
A full redraw copies 8 bytes for every cell, which this machine notices.

**Music for each scene.** This fits the slots without any extra work. The
player (`music.s`, about 0.9K with the sound effects) stays in core, because
it runs from the interrupt in every mode. The score is data, 132 bytes now,
and is loaded with each engine, inside the same pause: the map brings the
travelling tune (or one each for overworld, town and dungeon, loaded when
the scene changes), the fight a battle theme, talk a quieter one.
`tools/mkmusic.py` would write each score as a small file of its own, all
linked to the same address: a score area in core, which each load
overwrites. (The score could go inside the engine's own file instead, but
then each scene could have only one tune.) Because no
score has to stay resident, each one can be as long as you like.
`music_start()` would then take the score that was just loaded, and
`music_pause()`/`music_resume()` would become stop/start at a swap.

## Suggested order of experiments

1. **One score per scene**: teach `mkmusic.py` and `music.s` to play a score
   that was loaded rather than linked in. This is small, can be heard on its
   own, and gives every later swap its transition.
2. **Gather core state** into one block, save straight from it, and drop the
   save blob. This is useful whatever happens next.
3. **Approach A with the fight alone.** Move the shared bits of battle.c to
   core, turn `gear_drop` into a flag, link the fight as `q4k.fgt` into a slot
   at OVL's address, and add `far_call`. Measure with `tools/modes.py`.
4. **Move the services into the same slot**, one at a time, then the title.
5. **The data moves** (towns, FOV memory, parked continent), which free the
   RAM under the KERNAL.
6. **Go on to B**: split view and game into their core and map halves and
   put the map in the slot as well. Monsters on the map then grow the map
   engine inside a 16K slot that has room to spare, instead of taking from
   what A freed.
