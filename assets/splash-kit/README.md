# Splash screen kit

The `splash-320x184` picture and its line of text, in NLQ 12, as a routine your C64
program can call: `jsr splash_show`. It shows the picture until a key is pressed or a
few seconds pass, then gives the machine back as it was: KERNAL, interrupts and text
screen.

ca65 source (cc65). Made with vic2orious 0.1.0 on 2026-11-15.

## Contents

| file | |
|---|---|
| `lib/splash.inc` | `splash_show`, its options, and the includes for everything below |
| `lib/v2i.inc`, `lib/v2t.inc` | the image and text libraries ([docs/v2i-lib.md](docs/v2i-lib.md), [docs/v2t-lib.md](docs/v2t-lib.md)) |
| `lib/font.inc` | NLQ at 12 points, which the text was laid out for |
| `data/splash-p.v2i`, `data/splash-p.v2t` | the picture and its text for PAL C64s (20K) |
| `data/splash-n.v2i`, `data/splash-n.v2t` | the same for NTSC C64s (20K) |
| `splash-demo.s`, `splash-demo.prg` | an example host program, and the PRG built from it |
| `Makefile`, `c64.cfg` | builds `splash-demo.prg` with `cl65` |
| `docs/` | the library and file format documentation |

## Using it

```
        jsr splash_show                 ; picture and text until a key or time is up
        bcs no_splash                   ; C=1: none for this machine (A = error)
        ; C=0: A = the key that ended it (PETSCII), 0 if the time was up
        ...

        .include "splash.inc"           ; somewhere in RAM below $c000
```

Assemble with the `lib` directory on the include path and `data` on the binary include
path:

```
cl65 -t none -C c64.cfg --asm-include-dir lib --bin-include-dir data -o your.prg your.s
```

`splash.inc` assembles code and data into the current segment, so include it in one
your program loads into RAM below `$c000`, after the code that calls it (as at the end of
`splash-demo.s`). It has no segment directives of its own.

### Keys

`A` holds the key that ended the splash, as the KERNAL would give it:

| key | A |
|---|---|
| (time was up) | `$00` |
| RETURN | `$0d` |
| space | `$20` |
| F1, F3, F5, F7 | `$85`, `$86`, `$87`, `$88` |

The picture's text says "F1 Start New Game, F3 Load Saved Game", so a program can act on
`$85` and `$86` straight away. Other keys are ignored. A key held down when the splash
starts doesn't count, and `splash_show` waits for the key to be released before it
returns, so your program doesn't see it again. Keys are read from the keyboard matrix
(CIA1), because the KERNAL's keyboard scan is off while the picture is shown. A joystick
in port 1 shares those lines and can look like a key press.

### Options

Define these before the include (`NAME = value`), or pass them to `cl65` with
`--asm-define NAME=value`:

| option | default | |
|---|---|---|
| `SPLASH_SECS` | 5 | seconds on screen, 1-1000; 0: until a key is pressed |
| `SPLASH_KEY` | 1 | 0: keys don't end it, only the time does |
| `SPLASH_PAL` | 1 | 0: leave out the PAL picture (20K less) |
| `SPLASH_NTSC` | 1 | 0: leave out the NTSC picture (20K less) |

Each picture only works on its own video system, so with both in (41K) the splash
shows on every C64. `splash_show` finds out which system it's running on. If you leave
one out, a machine of that kind skips the splash: `splash_show` returns at once with
C=1 and `A = V2I_ERR_SYSTEM` (2). It does the same on a VIC-II the library doesn't
support, such as the early NTSC 6567R56A.

## What it uses

While the picture is shown, `splash_show` owns the machine: the KERNAL is switched out,
the picture's own raster interrupt runs, and all 8 sprites are in use. Nothing else of
your program runs until it returns.

When it returns, these are as they were before: the VIC registers and bank, `$01`,
interrupts and the CIA1 timer interrupt for the KERNAL, and v2i's zero page block,
`$10-$1c` (BASIC's).

These are overwritten:

| memory | |
|---|---|
| `$02` | used by the picture's display code |
| `$1d-$29` | v2t's zero page (BASIC's temporary area; to move it, define `V2T_ZP` before the include) |
| RAM at `$c000-$ffff` | the picture is unpacked here (VIC bank 3), and its interrupt vectors are at `$fffa-$ffff` (RAM under the KERNAL ROM): don't keep anything there during the splash; it's free again afterwards |

Your text screen at `$0400` and colour RAM are left alone, so the screen comes back as
it was.

The `.v2i` and `.v2t` data must stay in memory while the splash is shown (the display
code runs from inside the `.v2i`), but it can be overwritten once `splash_show`
returns. To load the splash from disk instead of building it in, load the files
anywhere below `$c000` and call the libraries yourself (see `docs/v2i-lib.md`).

## Building the example

```
make                    # splash-demo.prg: both pictures, 5 seconds or a key
make SECS=0             # until a key
make PAL=0              # NTSC only (or NTSC=0)
make run                # start it in VICE (x64sc)
```

`splash-demo.prg` shows the splash, then prints what ended it ("TIME WAS UP", or the
key) and returns to BASIC.

## Checked

The splash matched the converter's model of the screen in VICE with 0 differing pixels,
on PAL and NTSC. The return to BASIC after the time was up was checked on both, and so
was the fallback when a machine's picture is left out. The key scan was tested on its
own with a simulated keyboard: each of the six keys gives its code, and other keys give
0. The keys haven't been pressed on a real or emulated C64, because VICE's
`-keybuf` can't press keys in the matrix.

## Licences

The libraries (`v2i.inc`, `v2t.inc`, `splash.inc`) are Apache-2.0 (`LICENSE`).
`font.inc` is made from the GEOS font NLQ, which is not part of vic2orious.

## Changing the picture or the text

Edit `input/splash-320x184.txt` (or the picture) in vic2orious, then build both systems
with the same font and copy the new files over the ones in `data/`. Rebuild
`lib/font.inc` too if you change the font. `v2t_draw` doesn't draw a text laid out
for a font of another height.

```
make DUAL=1 FONT=fonts/NLQ.cvt:12
cp build/pal/splash-320x184.v2i  <kit>/data/splash-p.v2i
cp build/pal/splash-320x184.v2t  <kit>/data/splash-p.v2t
cp build/ntsc/splash-320x184.v2i <kit>/data/splash-n.v2i
cp build/ntsc/splash-320x184.v2t <kit>/data/splash-n.v2t
python3 -c "from vic2 import font; font.use('fonts/NLQ.cvt:12'); open('<kit>/lib/font.inc', 'w').write(font.current().asm())"
```
