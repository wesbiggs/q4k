# Quest for the King — working notes

Grid-based RPG roguelike for the Commodore 64 (Ultima III lineage).
cc65 + VICE on macOS. See README.md for the workflow and the story.

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

- `$0801-$37FF` **MAIN** — code and rodata. Twelve kilobytes, and the game
  does not fit in it.
- `$3800-$3FFF` **charset**, built at runtime. Must be in VIC bank 0 and not
  at `$1000`/`$1800`, where the VIC sees character ROM whatever RAM holds.
- `$4000-$9FFF` **HIGH** — `HIDATA` for bulk tables (the overworld, the
  towns) and `HICODE` for module code that will not fit below. Modules listed
  in the Makefile's `HIMODS` are compiled with `--code-name HICODE`; add to
  that list when MAIN overflows. Changing the list needs the objects rebuilt,
  because a target-specific `CFLAGS` will not invalidate them on its own.
- `$A000-$C4FF` **BSS**, then the RS-232 buffers, C stack and probe page.
- `$0340-$03FF` **hero sprite** data (blocks 13 to 15, in the cassette
  buffer). Sprites must sit in VIC bank 0 like the charset; all three are in
  use. Nothing else may use the tape buffer.
- More sprite blocks come out of the **charset's reverse half**, which the
  game never draws except `$A0` (the input cursor). Glyphs `$C0-$C7` are
  `$3E00-$3E3F`, the bow shot's block. Any 8 aligned glyphs from `$80-$FF`
  (not `$A0-$A7`) make another, but check `assets/tiles.txt` first: monster
  glyphs live at `$80-$8B`.

The PRG is one contiguous load, so MAIN and the charset gap are `fill = yes`.
That is why the small tools use `cfg/c64-tool.cfg` instead: without it a
1.4K hello world becomes a 14K one.

## Things that will bite you

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

## Testing

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
`reseed()` draws from. `dungeon_build()` seeds deliberately, for repeatable
levels, so anything that calls it reseeds afterwards.

`src/rngtest.c` measures `rand()`'s distribution after `srand()`; it is what
ruled the generator out when the encounter rate looked wrong.

**NPCs will invent geography** unless you hand them the real thing.
`tools/mkgeo.py` measures bearings and walking distances off the actual map
into `assets/geography.json`, and the daemon gives each speaker only the
lines for where they stand, plus an instruction not to guess at the rest.
Anything that moves a place on the map must regenerate it -- `make` does,
and `assets/places.txt` makes a moved dungeon a hard error rather than a
wrong direction.

**Measure before optimising on this machine; the intuitions do not
transfer.** Field of view clears the squares lit last turn, and the obvious
6502 move -- walk the array with a pointer instead of an index -- measured
*slower* than the indexed loop it replaced (27.1ms against 18.5ms), because
cc65 keeps the pointer out of zero page. What actually helped was not
touching all 399 squares at all: keeping a list of what was lit took it to
4.4ms. `--peek` plus the jiffy clock at `$A2` over 64 iterations is enough
resolution to settle questions like this in a couple of minutes.

**Disk access needs MEMSIZ raised first.** `$0283` points at the top of
BASIC memory, which is `$A000` on a stock machine — exactly where this
game's BSS starts. Leave it there and KERNAL SAVE writes nothing and LOAD
walks into the weeds, both without an error you can see. `savegame.c` and
`modem.c` both raise it to `$C700` before touching the drive.

**`cbm_save`'s last argument is a byte count, not an end address** — unlike
almost every other save routine on this machine. Passing an end address
produces a two-byte file containing only the load address, and a silent one:
the call returns success.

**A screen test that walks in the open is flaky by construction.** Overworld
and dungeon squares roll for wandering monsters, and the seed now comes from
the player's own timing, so the roll differs every run. Walk inside a town:
towns never roll. This is what turned the save test red one run in several,
with a rat on the screen instead of the map.

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

**The Makefile does not track headers.** Objects depend on their `.c`
only, so after anything regenerates `include/q4k/*.h` (a new tile moves
`TILE_COUNT`, say) run `rm -rf build/lib build/*.o` before building, or
stale objects will quietly use the old values. New furniture drew in the
keeper's red this way.

**Open question: do three sprites upset disk access?** When the tunic went
in as a third sprite, the save/load tests failed 7 runs in 20 against 20 in
20 with two sprites -- but another session was editing and testing in the
same checkout at the time, so the comparison is not trustworthy. Recheck
with the machine otherwise idle, around 20 runs of each case. If three
sprites really do fail, hide them during disk access in `savegame.c`: every
enabled sprite takes CPU cycles from the KERNAL's serial-bus code.

`--peek SYM[+OFF][:WIDTH]` prints any global after the run; `--poke
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
