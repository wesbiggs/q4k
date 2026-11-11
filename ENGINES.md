# Engines: what could come off the disk

An exploration, not a plan of record. The question: with a fast drive
(TeensyROM's vdrive), how much memory can the offline build free by keeping
only a small core resident and loading each mode's code and data when that
mode starts? Everything here is measured from the offline build as of 0.11.0
(`make offline`, then `tools/modes.py`), and the tool is there so that every
experiment on this branch can be measured the same way.

## The short answer

| Approach | Freed (about) | Load per fight | Rework |
|---|---|---|---|
| A. One swap slot; the map stays resident | about 9K | the fight, ~9K | small |
| B. The map is swapped out too | 17K | the fight in, the map back, ~22K | moderate |
| Data only (towns, arenas, help text, FOV memory) | 2-3K | none | small |

On top of all of that, **about 6K under the KERNAL is unused today**
(`$E7C0-$FFF9`, vault slots 1 to 3), and with the disk, slot 0 can be freed
as well.

Both A and B work through the same mechanism, a *far call*, and you can do A
first and B later. Neither works unless LOAD is fast, which
[the caveat](#the-caveat-load-speed) covers.

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

## The caveat: load speed

Every number above assumes LOAD is fast. On a stock 1541 the KERNAL loads
about 400 bytes a second, so a 9K fight would take more than 20 s to load,
and approach B could not work at all. Whether TeensyROM's vdrive gets close to
instant depends on whether it serves the KERNAL's own serial routines or
replaces them. Measure it on the real machine before building on it: a
40-line program that loads a 9K file and prints the jiffy clock (`$A2`)
would settle it. In VICE, tests run in warp mode, so they will not show the
problem. Use `+truedrive` (or `-virtualdev`) for a fast case, and true drive
emulation without warp for a slow one.

Two other costs:

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
spending some of the freed 9K on the map itself.

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

## Suggested order of experiments

1. **Measure load speed** on the TeensyROM and in VICE (above). Everything
   hinges on it.
2. **Gather core state** into one block, save straight from it, and drop the
   save blob. This is useful whatever happens next.
3. **Approach A with the fight alone.** Move the shared bits of battle.c to
   core, turn `gear_drop` into a flag, link the fight as `q4k.fgt` into a slot
   at OVL's address, and add `far_call`. Measure with `tools/modes.py`.
4. **Move the services into the same slot**, one at a time, then the title.
5. **The data moves** (towns, FOV memory, parked continent), which free the
   RAM under the KERNAL.
6. Then decide between staying with A and going on to B, based on what the
   first features want: monsters on the map want A's resident map, and
   portraits and bitmaps want B's single large slot.
